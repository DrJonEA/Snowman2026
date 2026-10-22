/**
 * PDX-License-Identifier: Apache-2.0
 * Copyright 2026 Keywave Technology Limited
 */

/**
 * KW307 SDK - Runtime / IO Layer - Implementation
 * FW: 2.7.5
 *
 * Owns the SDK's singleton state (s_uart / callback / blocking read).
 * The feature layer (KW307_api.c) and the CMD encoder layer
 * (KW307_cmd.c) reach this state ONLY via the non-public accessor
 * kw307_default_uart() — declared inline in each consumer file, never
 * promoted to a header.
 */

#include "KW307_io.h"
#include "KW307_uart.h"
#include "KW307_port.h"
#include <string.h>


/* ================================================================
   Internal state - singleton
   ================================================================ */

static KW307Uart_t  s_uart;

/* Output callbacks — one per typed periodic frame. Each fires from the
 * receive dispatcher when a matching wire CMD byte arrives. */
static void (*s_cb_frame)        (const KW307Frame_t*);          /* CMD 0xF0 (default frame) */

/* Output-frame handler registry — feature add-ons register here (see
 * kw307_register_output_handler). Base io dispatches any output frame whose
 * wire CMD matches a registered handler, without knowing the module exists. */
#define KW307_MAX_OUTPUT_HANDLERS 4
static struct { uint8_t cmd; KW307OutputHandler_t h; } s_out[KW307_MAX_OUTPUT_HANDLERS];

/* Blocking-read state (see kw307_read_blocking). */
#define KW307_READ_BUF_MAX  64
static volatile struct {
    uint8_t  waiting_cmd;    /* 0 = idle, else CMD being awaited */
    uint8_t  got_response;   /* 1 = data frame captured */
    uint8_t  got_ack_fail;   /* 1 = ACK with non-OK status */
    uint8_t  ack_status;     /* MCU STATUS byte from last NACK (latched) */
    uint16_t resp_len;
    uint8_t  resp_buf[KW307_READ_BUF_MAX];
} s_read;


/* Singleton accessor — KW307_api.c and KW307_cmd.c reach the UART context
 * via this function (each declares it locally as extern). Intentionally
 * NOT exposed in any header so the singleton stays an internal concept of
 * the api/cmd/io layer triad. */
KW307Uart_t* kw307_default_uart(void) { return &s_uart; }


/* Forward declaration */
static void on_frame(uint8_t sof2, uint8_t cmd,
                     const uint8_t* data, uint16_t len, void* user);


/* ================================================================
   Lifecycle
   ================================================================ */

int kw307_init(void)
{
    memset(&s_uart, 0, sizeof(s_uart));
    s_cb_frame          = NULL;
    /* Full reset of the blocking-read slot so a previous session's NACK status
     * or response bytes can't leak into the new connection (e.g. when the app
     * reconnects after a USB unplug). */
    s_read.waiting_cmd  = 0u;
    s_read.got_response = 0u;
    s_read.got_ack_fail = 0u;
    s_read.ack_status   = 0u;
    s_read.resp_len     = 0u;
    memset((void*)s_read.resp_buf, 0, sizeof(s_read.resp_buf));

    kw307_uart_init(&s_uart, on_frame, NULL);

    /* port_name=NULL, baud_rate=0: each platform reads from user_config.h */
    if (kw307_uart_open(&s_uart, NULL, 0) != 0)
        return KW307_ERR_PORT_OPEN;

    return KW307_OK;
}

uint8_t kw307_last_nack_status(void)
{
    return s_read.ack_status;   /* latched until next NACK overwrites */
}

int kw307_close(void)
{
    kw307_uart_close(&s_uart);
    /* Clear the blocking-read slot too so a subsequent reconnect via
     * kw307_init() starts from a clean state — guards against a stray frame
     * arriving in the window between close and the next init landing in a
     * stale capture slot. */
    s_read.waiting_cmd  = 0u;
    s_read.got_response = 0u;
    s_read.got_ack_fail = 0u;
    return KW307_OK;
}

bool kw307_is_open(void)
{
    return kw307_port_is_open() != 0;
}


/* ================================================================
   Output delivery
   ================================================================ */

void kw307_on_frame        (void (*cb)(const KW307Frame_t*))        { s_cb_frame     = cb; }

int kw307_register_output_handler(uint8_t cmd, KW307OutputHandler_t handler)
{
    int free_i = -1;
    if (cmd == 0u || handler == NULL) return KW307_ERR_PARAM;
    for (int i = 0; i < KW307_MAX_OUTPUT_HANDLERS; i++) {
        if (s_out[i].cmd == cmd) { s_out[i].h = handler; return KW307_OK; }  /* replace existing */
        if (s_out[i].cmd == 0u && free_i < 0) free_i = i;                    /* remember a free slot */
    }
    if (free_i < 0) return KW307_ERR_PARAM;   /* table full */
    s_out[free_i].cmd = cmd;
    s_out[free_i].h   = handler;
    return KW307_OK;
}


/* ================================================================
   RX update (MCU / Arduino main loop)
   ================================================================ */

int kw307_update(void)
{
    int r = kw307_uart_poll(&s_uart, 0);
    return r < 0 ? KW307_ERR_PORT : r;
}


/* ================================================================
   Blocking read - generic synchronous request/response
   ================================================================ */

int kw307_read_blocking(uint8_t cmd,
                        const uint8_t* req_data, uint16_t req_len,
                        uint8_t* resp_buf, uint16_t resp_buf_max,
                        uint16_t* resp_len,
                        int timeout_ms)
{
    int elapsed;

    if (!resp_buf || resp_buf_max == 0)     return KW307_ERR_PARAM;
    if (req_len > 0 && !req_data)           return KW307_ERR_PARAM;
    if (cmd == 0u)                          return KW307_ERR_PARAM;

    if (!kw307_port_is_open()) return KW307_ERR_PORT_CLOSED;

    /* Arm capture slot */
    s_read.got_response = 0u;
    s_read.got_ack_fail = 0u;
    s_read.resp_len     = 0u;
    s_read.waiting_cmd  = cmd;

    if (kw307_uart_send_read(&s_uart, cmd, req_data, req_len) != 0) {
        s_read.waiting_cmd = 0u;
        return KW307_ERR_PORT_SEND;
    }

    /* Poll until response, rejection, port-down, or timeout. */
    for (elapsed = 0; elapsed < timeout_ms; elapsed++) {
        kw307_uart_poll(&s_uart, 1);

        if (s_read.got_response) {
            uint16_t n = (s_read.resp_len < resp_buf_max)
                       ?  s_read.resp_len : resp_buf_max;
            for (uint16_t i = 0; i < n; i++) resp_buf[i] = s_read.resp_buf[i];
            if (resp_len) *resp_len = n;
            s_read.waiting_cmd = 0u;
            return KW307_OK;
        }
        if (s_read.got_ack_fail) {
            s_read.waiting_cmd = 0u;
            return KW307_ERR_NACK;   /* MCU rejected — see kw307_last_nack_status() */
        }
        /* Fail fast if the port dies mid-wait (USB unplug, device removal) —
         * the port impl's send/recv error path invalidates the handle, so we
         * stop spinning and return PORT_CLOSED instead of waiting out the full
         * timeout_ms only to report TIMEOUT. */
        if (!kw307_port_is_open()) {
            s_read.waiting_cmd = 0u;
            return KW307_ERR_PORT_CLOSED;
        }
    }

    s_read.waiting_cmd = 0u;
    return KW307_ERR_TIMEOUT;
}


/* ================================================================
   Internal frame router - dispatches incoming output frames to the
   registered callback, and captures blocking-read responses.
   ================================================================ */

static void on_frame(uint8_t sof2, uint8_t cmd,
                     const uint8_t* data, uint16_t len,
                     void* user)
{
    (void)user;

    /* --- Output frames (AA 55) ---
     * Dispatched by wire CMD byte. Each frame type only fires its own
     * callback; an unregistered callback simply drops the frame. */
    if (cmd == KW307_CMD_OUT_FRAME) {
        KW307Frame_t frame;
        if (kw307_dec_frame(data, len, &frame) == 0 && s_cb_frame) {
            s_cb_frame(&frame);
        }
        return;
    }
    /* --- Output frames (AA 55) claimed by an optional feature module via
     *     kw307_register_output_handler. Base io stays unaware of them. --- */
    if (sof2 == KW307_SOF_WRITE) {
        for (int i = 0; i < KW307_MAX_OUTPUT_HANDLERS; i++) {
            if (s_out[i].cmd == cmd && s_out[i].h) {
                s_out[i].h(cmd, data, len);
                return;
            }
        }
    }

    /* --- Read responses / ACKs (AA 56): capture for kw307_read_blocking --- */
    if (sof2 == KW307_SOF_READ && s_read.waiting_cmd != 0u) {
        if (cmd == KW307_CMD_ACK && len >= 2u) {
            /* ACK for read: data[0]=acked_cmd, data[1]=status.
             * Success ACK (status=0) is followed by a separate data frame;
             * failure ACK means no data frame will arrive. Latch the status
             * byte so the caller can fetch it via kw307_last_nack_status(). */
            if (data[0] == s_read.waiting_cmd && data[1] != KW307_STATUS_OK) {
                s_read.ack_status   = data[1];
                s_read.got_ack_fail = 1u;
            }
        } else if (cmd == s_read.waiting_cmd) {
            uint16_t n = (len < KW307_READ_BUF_MAX) ? len : KW307_READ_BUF_MAX;
            for (uint16_t i = 0; i < n; i++) s_read.resp_buf[i] = data[i];
            s_read.resp_len     = n;
            s_read.got_response = 1u;
        }
    }
}
