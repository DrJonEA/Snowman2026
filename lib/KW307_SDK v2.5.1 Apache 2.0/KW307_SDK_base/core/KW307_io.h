/**
 * PDX-License-Identifier: Apache-2.0
 * Copyright 2026 Keywave Technology Limited
 */


/**
 * KW307 SDK - Runtime / IO Layer
 * FW: 2.7.5
 *
 * The MINIMUM surface needed to drive the SDK and receive radar frames.
 *
 *   - kw307_init()                  once at startup
 *   - kw307_update()                every main-loop iteration
 *   - kw307_on_frame()              register your callback to consume frames
 *
 * BASIC USAGE (no configuration, MCU runs with NVM defaults):
 *   #include "KW307_io.h"
 *
 *   static void on_radar(const KW307Frame_t* f) {
 *       GPIO_SET(LED, f->human_flag);          // presence on/off
 *       // f->moving[0].distance_cm / .angle_tenth also available
 *   }
 *
 *   int main(void) {
 *       kw307_init();
 *       kw307_on_frame(on_radar);
 *       while (1) kw307_update();
 *   }
 *
 * For configuration CMDs (gain, range, FOV, sensitivity, factory reset,
 * firmware-version check, ...) additionally #include "KW307_api.h" — the
 * feature layer that sits on top of this one.
 */

#ifndef KW307_IO_H
#define KW307_IO_H

#include <stdint.h>
#include <stdbool.h>
#include "KW307_protocol.h"   /* KW307Frame_t */

#ifdef __cplusplus
extern "C" {
#endif


/* ================================================================
   Error codes
   KW307_ERR_PORT (-1) is the generic UART failure; the sub-codes below give
   finer diagnosis. `if (rc != KW307_OK)` catches every error; switch on a
   specific value when you need to tell failures apart.
   ================================================================ */
typedef enum {
    KW307_OK              =   0,   /* success */

    /* --- Port (UART) layer --- */
    KW307_ERR_PORT        =  -1,   /* generic UART failure */
    KW307_ERR_PORT_OPEN   = -10,   /* kw307_port_open() returned non-zero */
    KW307_ERR_PORT_CLOSED = -11,   /* API called before init, or port lost */
    KW307_ERR_PORT_SEND   = -12,   /* kw307_port_send returned short / negative */
    KW307_ERR_PORT_RECV   = -13,   /* kw307_port_recv returned negative */

    /* --- Argument / capability --- */
    KW307_ERR_PARAM       =  -2,   /* argument out of valid range */
    KW307_ERR_NOT_IMPL    =  -3,   /* feature not yet implemented in this SDK build */

    /* --- Blocking-response layer (kw307_read_blocking and friends) --- */
    KW307_ERR_TIMEOUT     =  -5,   /* no response within timeout_ms */
    KW307_ERR_NACK        = -20,   /* MCU ACK with STATUS != OK; see kw307_last_nack_status() */
    KW307_ERR_RESP_LEN    = -21,   /* response payload shorter than expected */
    KW307_ERR_RESP_CRC    = -22,   /* response CRC-16 mismatch */
} KW307Err_t;


/* ================================================================
   Lifecycle
   ================================================================ */

/**
 * Initialise and open the device. Must be called once before any other
 * SDK function. Port name and baud rate are read from user_config.h.
 *
 * Returns KW307_OK on success.
 * Returns KW307_ERR_PORT_OPEN if the port cannot be opened.
 */
int  kw307_init(void);

/**
 * After a -KW307_ERR_NACK return, fetch the MCU STATUS byte that came back
 * in the ACK frame (0x01=invalid value, 0x02=unknown CMD, 0x03=CRC error,
 * 0x04=invalid length, etc.). Returns 0 if no NACK has occurred since last
 * clear; the value is latched until the next NACK overwrites it.
 */
uint8_t kw307_last_nack_status(void);

/** Close the port. */
int  kw307_close(void);

/** Returns true if the port is currently open. */
bool kw307_is_open(void);


/* ================================================================
   Output delivery — register a callback; it fires on every radar
   frame. The callback runs in the same context as kw307_update()
   (typically your main loop on MCU). Keep it short — latch fields
   into your own variables and act on them in the main loop.
   ================================================================ */

/**
 * Register a per-frame callback for the default 27-byte frame (wire CMD 0xF0,
 * output mode 0x00). Pass NULL to unregister. The pointer is valid only
 * during the callback — copy it out if you need it later. Called from
 * inside kw307_update().
 *
 * Use a target slot only when its state is Valid — see KW307Tracker_t.state.
 */
void kw307_on_frame(void (*cb)(const KW307Frame_t*));

/* Additional periodic-output frame callbacks may be provided by optional
 * feature modules, which register themselves via the extension point below. */


/* ================================================================
   Output-frame extension point (for optional feature modules)
   ----------------------------------------------------------------
   A feature module registers a handler for its own periodic-output wire CMD;
   base io dispatches matching frames to it without needing to know the module
   exists. The default frame (CMD 0xF0) is delivered via kw307_on_frame() and
   does not use this hook.
   Returns KW307_OK, or KW307_ERR_PARAM (cmd==0, NULL handler, or table full).
   ================================================================ */
typedef void (*KW307OutputHandler_t)(uint8_t cmd, const uint8_t* data, uint16_t len);
int kw307_register_output_handler(uint8_t cmd, KW307OutputHandler_t handler);


/* ================================================================
   RX update — call repeatedly on MCU / Arduino
   ----------------------------------------------------------------
   Pulls bytes from the platform UART, runs them through the frame
   parser, and fires registered callbacks for any complete frame.
   Non-blocking — returns immediately when the RX buffer is empty.

   Returns number of bytes consumed (>=0), or KW307_ERR_PORT on error.
   ================================================================ */
int kw307_update(void);


/* ================================================================
   Blocking read (synchronous request/response)
   ----------------------------------------------------------------
   Sends a Read request (AA 56) and blocks until the device responds
   or timeout_ms expires. Intended for setup-time queries and MCU
   main-loop usage. Typed Read wrappers in KW307_api.h use this
   internally — call directly only for CMDs that have no typed wrapper.
   ================================================================ */
int kw307_read_blocking(uint8_t cmd,
                        const uint8_t* req_data, uint16_t req_len,
                        uint8_t* resp_buf, uint16_t resp_buf_max,
                        uint16_t* resp_len,
                        int timeout_ms);


#ifdef __cplusplus
}
#endif

#endif /* KW307_IO_H */
