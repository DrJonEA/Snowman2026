/**
 * PDX-License-Identifier: Apache-2.0
 * Copyright 2026 Keywave Technology Limited
 */

/**
 * KW307_cmd.c - UART Command Layer (internal)
 * FW: 2.7.5
 *
 * Encodes each CMD payload (Big-Endian) and sends via kw307_uart_send_write().
 */

#include "KW307_cmd.h"
#include "KW307_io.h"     /* kw307_read_blocking, KW307Err_t */
#include "KW307_port.h"   /* kw307_port_is_open */
#include <stddef.h>       /* NULL */

/** @cond INTERNAL */

/* ================================================================
   Internal encode helpers
   ================================================================ */

static int cmd1(KW307Uart_t* u, uint8_t cmd, uint8_t val)
{
    return kw307_uart_send_write(u, cmd, &val, 1);
}

/* ================================================================
   System CMDs
   ================================================================ */

int kw307_cmd_read_fw_version(int timeout_ms,
                               uint8_t* major, uint8_t* minor, uint8_t* patch)
{
    uint8_t  resp[3];
    uint16_t n = 0;
    int ret = kw307_read_blocking(KW307_CMD_FW_VERSION, NULL, 0,
                                  resp, sizeof(resp), &n, timeout_ms);
    if (ret != KW307_OK) return ret;
    if (n < 3)           return KW307_ERR_RESP_LEN;
    if (major) *major = resp[0];
    if (minor) *minor = resp[1];
    if (patch) *patch = resp[2];
    return KW307_OK;
}

int kw307_cmd_factory_reset(KW307Uart_t* u)
{
    return cmd1(u, KW307_CMD_FACTORY_RESET, 0x01u);
}

int kw307_cmd_save_settings(KW307Uart_t* u)
{
    return cmd1(u, KW307_CMD_SAVE_SETTINGS, 0x01u);
}

int kw307_cmd_clear_state(KW307Uart_t* u)
{
    return cmd1(u, KW307_CMD_CLEAR_STATE, 0x01u);
}

/* ================================================================
   Customer CMDs
   ================================================================ */

int kw307_cmd_power_mode  (KW307Uart_t* u, uint8_t mode)   { return cmd1   (u, KW307_CMD_POWER_MODE,   mode);   }
int kw307_cmd_gain        (KW307Uart_t* u, uint8_t index)  { return cmd1   (u, KW307_CMD_GAIN,         index);  }

/* CMD 0x11 — customer-subset encoder. See KW307_cmd.h. */
int kw307_cmd_output_mode(KW307Uart_t* u, uint8_t mode)
{
    /* Customer output set (this SDK pairs with MCU FW 2.6.0):
     *   0x00 default frame / 0xFF off. Other mode values are enabled by
     *   optional feature modules. */
    if (mode > 0x02u && mode != 0xFFu) return KW307_ERR_PARAM;
    return cmd1(u, KW307_CMD_OUTPUT_MODE, mode) == 0 ? KW307_OK : KW307_ERR_PORT;
}

int kw307_cmd_detect_range(KW307Uart_t* u, uint16_t min_mm, uint16_t max_mm)
{
    (void)u;
    int r = kw307_set_parameter(0x0Fu, (uint8_t)(min_mm >> 8), (uint8_t)(min_mm & 0xFFu), 2u);
    if (r) return r;
    return    kw307_set_parameter(0x10u, (uint8_t)(max_mm >> 8), (uint8_t)(max_mm & 0xFFu), 2u);
}

int kw307_cmd_detect_fov(KW307Uart_t* u, int16_t deg10, uint8_t shrink_deg10)
{
    (void)u;
    int r = kw307_set_parameter(0x11u, (uint8_t)((uint16_t)deg10 >> 8),
                                  (uint8_t)((uint16_t)deg10 & 0xFFu), 2u);
    if (r) return r;
    return    kw307_set_parameter(0x12u, 0u, shrink_deg10, 2u);
}

int kw307_cmd_signal_threshold(KW307Uart_t* u, int16_t moving, int16_t stationary)
{
    (void)u;
    int r = kw307_set_parameter(0x24u, (uint8_t)((uint16_t)moving >> 8),
                                  (uint8_t)((uint16_t)moving & 0xFFu), 2u);
    if (r) return r;
    return    kw307_set_parameter(0x25u, (uint8_t)((uint16_t)stationary >> 8),
                                  (uint8_t)((uint16_t)stationary & 0xFFu), 2u);
}

int kw307_cmd_doppler_filter(KW307Uart_t* u,
                              uint8_t fhpf_khpf, uint8_t fhpf_nd, uint8_t flpf,
                              uint8_t shpf_khpf, uint8_t shpf_nd, uint8_t slpf)
{
    uint8_t buf[6] = { fhpf_khpf, fhpf_nd, flpf, shpf_khpf, shpf_nd, slpf };
    return kw307_uart_send_write(u, KW307_CMD_DOPPLER_FILTER, buf, 6);
}

static int send_tracker_pids(uint8_t pid_cnt_max, uint8_t pid_cnt_show,
                              uint8_t cnt_max, uint8_t cnt_show_th)
{
    int r;
    r = kw307_set_parameter(pid_cnt_max,   0u, cnt_max,     1u);
    if (r) return r;
    return kw307_set_parameter(pid_cnt_show, 0u, cnt_show_th, 1u);
}

int kw307_cmd_moving_tracker_cnt(KW307Uart_t* u, uint8_t cnt_max, uint8_t cnt_show_th)
{
    (void)u;
    return send_tracker_pids(0x2Eu, 0x2Fu, cnt_max, cnt_show_th);
}

int kw307_cmd_stationary_tracker_cnt(KW307Uart_t* u, uint8_t cnt_max, uint8_t cnt_show_th)
{
    (void)u;
    return send_tracker_pids(0x29u, 0x2Au, cnt_max, cnt_show_th);
}


/* ================================================================
   Algorithm parameter (CMD 0x90 — single PID write)
   ================================================================ */

/* Forward declaration of the SDK's singleton UART accessor (implemented in
 * KW307_api.c — kept out of any header so the singleton stays an internal
 * concept of the api/cmd layers, not part of the public surface). */
extern KW307Uart_t* kw307_default_uart(void);

int kw307_set_parameter(uint8_t pid, uint8_t d1, uint8_t d0, uint8_t len)
{
    if (len == 0u || len > 2u) return -1;
    uint8_t buf[3];
    buf[0] = pid;
    if (len == 2u) {
        buf[1] = d1;   /* HI */
        buf[2] = d0;   /* LO */
    } else {           /* len == 1 */
        buf[1] = d0;   /* the value byte */
    }
    return kw307_uart_send_write(kw307_default_uart(),
                                  KW307_CMD_ALGO_PARAM,
                                  buf, (uint16_t)(1u + len));
}

/* ---- Generic raw CMD escape hatches (declared in KW307_cmd.h) ---- */

int kw307_send_cmd(uint8_t cmd, const uint8_t* data, uint16_t len)
{
    if (!data && len > 0) return KW307_ERR_PARAM;
    if (!kw307_port_is_open()) return KW307_ERR_PORT_CLOSED;
    return kw307_uart_send_write(kw307_default_uart(), cmd, data, len) == 0
           ? KW307_OK : KW307_ERR_PORT_SEND;
}

int kw307_read_cmd(uint8_t cmd, const uint8_t* data, uint16_t len)
{
    if (!data && len > 0) return KW307_ERR_PARAM;
    if (!kw307_port_is_open()) return KW307_ERR_PORT_CLOSED;
    return kw307_uart_send_read(kw307_default_uart(), cmd, data, len) == 0
           ? KW307_OK : KW307_ERR_PORT_SEND;
}

/** @endcond */

