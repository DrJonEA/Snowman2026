/**
 * PDX-License-Identifier: Apache-2.0
 * Copyright 2026 Keywave Technology Limited
 */

/**
 * KW307_cmd.h - UART Command Layer (internal)
 * FW: 2.7.5
 *
 * Each function encodes the payload (Big-Endian) and sends one UART Write frame.
 * No parameter validation - callers (KW307_api.c) are responsible.
 *
 * Internal header - include KW307_api.h instead.
 */

#ifndef KW307_CMD_H
#define KW307_CMD_H

#include <stdint.h>
#include "KW307_uart.h"
#include "KW307_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @cond INTERNAL */

/* ================================================================
   System CMDs (0x01~0x03 + 0x09)
   ================================================================ */

/* CMD 0x01 — blocking read of firmware version. Any out-pointer may be NULL.
 * Returns KW307_OK on success or a KW307_ERR_* value on failure. */
int kw307_cmd_read_fw_version  (int timeout_ms,
                                 uint8_t* major, uint8_t* minor, uint8_t* patch);

int kw307_cmd_factory_reset    (KW307Uart_t* u);                        /* CMD 0x02 */
int kw307_cmd_save_settings    (KW307Uart_t* u);                        /* CMD 0x03 */
int kw307_cmd_clear_state      (KW307Uart_t* u);                        /* CMD 0x09 */

/* ================================================================
   Customer CMDs

   Power / output / gain / doppler ride dedicated CMD opcodes. The other
   encoders below fan out into CMD 0x90 PID writes (the trailing comment
   shows which PIDs each one sends).
   ================================================================ */
int kw307_cmd_power_mode       (KW307Uart_t* u, uint8_t mode);          /* CMD 0x10 */
int kw307_cmd_gain             (KW307Uart_t* u, uint8_t index);         /* CMD 0x12 */

/* CMD 0x11 — output-mode write, customer subset (MCU FW 2.7.3):
 *   0x00 default frame / 0xFF off. Other mode values are enabled by optional
 *   feature modules.
 * Returns KW307_OK / KW307_ERR_PARAM / KW307_ERR_PORT.
 */
int kw307_cmd_output_mode      (KW307Uart_t* u, uint8_t mode);          /* CMD 0x11 */
int kw307_cmd_detect_range     (KW307Uart_t* u, uint16_t min_mm, uint16_t max_mm);  /* PID 0x0F+0x10 */
int kw307_cmd_detect_fov       (KW307Uart_t* u, int16_t deg10, uint8_t shrink_deg10);  /* PID 0x11+0x12 */
int kw307_cmd_signal_threshold (KW307Uart_t* u, int16_t moving, int16_t stationary);   /* PID 0x24+0x25 */
int kw307_cmd_doppler_filter   (KW307Uart_t* u,                                        /* CMD 0x17 (6B) */
                                 uint8_t fhpf_khpf, uint8_t fhpf_nd, uint8_t flpf,
                                 uint8_t shpf_khpf, uint8_t shpf_nd, uint8_t slpf);
int kw307_cmd_moving_tracker_cnt     (KW307Uart_t* u, uint8_t cnt_max, uint8_t cnt_show_th);  /* PID 0x2E+0x2F */
int kw307_cmd_stationary_tracker_cnt (KW307Uart_t* u, uint8_t cnt_max, uint8_t cnt_show_th);  /* PID 0x29+0x2A */

/* ----------------------------------------------------------------
   Algorithm parameter (CMD 0x90 — single PID write)
   ---------------------------------------------------------------- */

/** Write a single raw algorithm parameter (advanced).
 *    d0 = LOW byte, d1 = HIGH byte. len = 1 (1-byte value, d1 ignored — pass 0)
 *    or len = 2 (16-bit value, big-endian).
 *  Raw parameter numbers are firmware-specific and not part of the stable API —
 *  prefer the typed setters in KW307_api.h when one exists. */
int kw307_set_parameter(uint8_t pid, uint8_t d1, uint8_t d0, uint8_t len);

/* ----------------------------------------------------------------
   Raw / advanced CMD access (no typed wrapper).
   Generic escape hatches — kept in this internal header so they stay out
   of the customer-facing doxygen surface, but remain callable: including
   KW307_api.h pulls this header in. Read-style commands: pair with
   kw307_read_blocking() in KW307_io.h.
   ---------------------------------------------------------------- */

/** Send any command as a Write frame. data=NULL, len=0 for no payload. */
int kw307_send_cmd(uint8_t cmd, const uint8_t* data, uint16_t len);

/** Send any command as a Read-request frame. */
int kw307_read_cmd(uint8_t cmd, const uint8_t* data, uint16_t len);

/** @endcond */

#ifdef __cplusplus
}
#endif

#endif /* KW307_CMD_H */
