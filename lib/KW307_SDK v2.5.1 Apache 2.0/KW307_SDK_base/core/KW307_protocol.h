/**
 * PDX-License-Identifier: Apache-2.0
 * Copyright 2026 Keywave Technology Limited
 */


/**
 * KW307 SDK - Protocol Layer
 * FW: 2.7.5
 *
 * Defines:
 *   - CMD IDs
 *   - ACK status codes
 *   - Output data struct (self-contained, no algorithm dependency)
 *   - Decode: parse output frame DATA payload into struct
 *
 * Dependencies: stdint.h only - no OS, no VCL.
 */

#ifndef KW307_PROTOCOL_H
#define KW307_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
   System CMDs (0x01~0x03)
   ================================================================ */
#define KW307_CMD_FW_VERSION          0x01   /* Read only: [major][minor][patch] */
#define KW307_CMD_FACTORY_RESET       0x02   /* Write 0x01 to trigger */
#define KW307_CMD_SAVE_SETTINGS       0x03   /* Write 0x01 to persist settings to flash */
#define KW307_CMD_CLEAR_STATE         0x09   /* Write 0x01 to clear detection state (FW 2.7.2+) */

/* ================================================================
   Customer CMDs
   Same CMD ID used for Write (AA 55) and Read (AA 56).
   ================================================================ */
#define KW307_CMD_POWER_MODE          0x10   /* 1B: 0=Down, 1=Normal */
#define KW307_CMD_OUTPUT_MODE         0x11   /* 1B: see KW307_OUTPUT_xxx */
#define KW307_CMD_GAIN                0x12   /* 1B: 0~8 (-3dB ~ +21dB step 3) */
#define KW307_CMD_DOPPLER_FILTER      0x17   /* 6B doppler config (moving + stationary bands) */
#define KW307_CMD_ALGO_PARAM          0x90   /* PID write/read: 1+1 or 1+2 byte [pid][value(BE)] */

/* ACK response CMD */
#define KW307_CMD_ACK                 0xAC

/* Periodic output CMDs (MCU -> Host) */
#define KW307_CMD_OUT_FRAME           0xF0   /* default: 27B  human + 2 moving + 1 stationary */
/* Additional output-frame CMD IDs are defined by optional feature modules. */

/* ================================================================
   Power Mode values (CMD 0x10)
   ================================================================ */
#define KW307_POWER_DOWN              0
#define KW307_POWER_NORMAL            1
#define KW307_POWER_PA_OFF            2
#define KW307_POWER_SAVING            3

/* ================================================================
   Output Mode values (CMD 0x11)
   ================================================================ */
#define KW307_OUTPUT_FRAME            0x00   /* default — frame containing human_flag + 2 moving + 1 stationary target (decode: KW307Frame_t) */
/* Additional output-mode values are defined by optional feature modules. */
#define KW307_OUTPUT_OFF              0xFF   /* stop periodic UART output; radar still runs */

/* ================================================================
   ACK Status codes
   ================================================================ */
#define KW307_STATUS_OK               0x00
#define KW307_STATUS_INVALID          0x01   /* value out of range */
#define KW307_STATUS_UNKNOWN          0x02   /* unknown CMD */
#define KW307_STATUS_CRC_ERR          0x03   /* CRC-16 mismatch */
#define KW307_STATUS_LEN_ERR          0x04   /* wrong length */

/* ================================================================
   Output data structs - self-contained (no algorithm dependency)
   All fields Big-Endian in wire format; parsed to host-endian here.
   ================================================================ */
#pragma pack(push, 1)

/**
 * @brief One tracked target.
 *
 * Element type of KW307Frame_t::moving[] and KW307Frame_t::stationary[].
 * Wire format on UART is mm, but the SDK decoder converts to cm before
 * populating this struct so the customer sees whole-cm units (sub-cm
 * precision is truncated).
 *
 * The two arrays use different `state` value ranges (documented below).
 */
typedef struct {
    uint16_t distance_cm;    /**< Distance in cm (decoder: wire mm / 10). */
    int16_t  angle_tenth;    /**< Angle in deg x 10 (e.g. -350 = -35.0 deg). */
    uint16_t mag;            /**< Signal strength of the target in this slot, 0 when the
                              *   slot is Empty. Use `state`, not `mag`, to tell whether
                              *   a slot holds a target. */
    uint8_t  state;          /**< Slot status. Which form you get depends on
                              *   kw307_set_tracker_state_advanced_mode();
                              *   the simple form is the default.
                              *
                              *   Simple form (both arrays):
                              *     0 = Empty, 1 = Valid. An Empty slot also has
                              *     distance / angle / mag / counter zeroed.
                              *
                              *   Advanced form:
                              *     moving[]    : 0=Empty, 1=Checking, 2=Birth, 3=Valid.
                              *                   Checking / Birth are candidates, not yet
                              *                   confirmed. A target rejected as
                              *                   repetitive non-human motion (e.g. a fan)
                              *                   is reported as Empty.
                              *     stationary[]: 0=Empty, 1=Valid (visible), 2=Hidden.
                              *                   Hidden = a moving target is locked at
                              *                   the same spot; do NOT draw it, or one
                              *                   person shows as two dots.
                              *
                              *   A slot holds a real target at Valid: `state == 1` in the
                              *   simple form, and in the advanced form `state == 3` for
                              *   moving[] / `state == 1` for stationary[]. The remaining
                              *   non-zero values are not ready to display, so do not use
                              *   `state != 0` as the test. */
    uint8_t  counter;        /**< Age / confidence count, roughly 0~32. It rises while
                              *   the target keeps being detected and falls when it is
                              *   missed; the slot is released at 0. Useful as a
                              *   confidence or fade level. */
} KW307Tracker_t;

#pragma pack(pop)

/**
 * @brief Periodic radar frame delivered to your callback.
 *
 * Decoded from the MCU's auto-streamed frame output. Contains a frame
 * counter, a human-presence flag, two moving-target slots, and one
 * stationary-target slot. Pass-by-pointer in kw307_on_frame() callbacks;
 * the pointer is only valid for the duration of the callback - copy
 * fields out if you need them later.
 */
typedef struct {
    uint16_t       frame_index;     /**< Monotonic frame counter. */
    uint8_t        human_flag;      /**< 1 = presence detected this frame. */
    KW307Tracker_t moving[2];       /**< Two moving-target slots (walking, gesturing, ...). */
    KW307Tracker_t stationary[1];   /**< One stationary-target slot (still person, micro-motion). */
} KW307Frame_t;

/* Optional feature modules carry their own output-frame struct + decoder. */

/* ================================================================
   Decode - parse incoming output frame DATA payload
   Returns 0 on success, -1 if len is wrong.
   ================================================================ */
int kw307_dec_frame        (const uint8_t* data, uint16_t len, KW307Frame_t*        out);

/* Parse ACK frame: returns STATUS byte, or -1 if not an ACK frame */
int kw307_dec_ack(uint8_t cmd, const uint8_t* data, uint16_t len,
                  uint8_t* acked_cmd_out);

#ifdef __cplusplus
}
#endif

#endif /* KW307_PROTOCOL_H */
