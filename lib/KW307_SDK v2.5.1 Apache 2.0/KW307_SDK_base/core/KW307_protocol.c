/**
 * PDX-License-Identifier: Apache-2.0
 * Copyright 2026 Keywave Technology Limited
 */


/**
 * KW307 SDK - Protocol Layer - Implementation
 * FW: 2.7.5
 *
 * Decode only: parse Big-Endian wire bytes into host-endian structs.
 * CMD encode + send lives in KW307_cmd.c (internal cmd layer).
 */

#include "KW307_protocol.h"

/* ================================================================
   Internal helpers
   ================================================================ */

static int16_t dec_i16be(const uint8_t* p)
{
    return (int16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint16_t dec_u16be(const uint8_t* p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

/* ================================================================
   Internal: parse one Tracker from wire bytes
   ================================================================ */
static const uint8_t* parse_tracker(const uint8_t* p, KW307Tracker_t* t)
{
    t->distance_cm = (uint16_t)(dec_u16be(p) / 10u);   /* wire mm -> struct cm */
    p += 2;
    t->angle_tenth = dec_i16be(p);     p += 2;
    t->mag         = dec_u16be(p);     p += 2;  /* IIR-filtered magnitude */
    t->state       = *p++;
    t->counter     = *p++;
    return p;
}

/* ================================================================
   Decode - periodic radar frame (default, wire CMD 0xF0)
   ================================================================ */
int kw307_dec_frame(const uint8_t* data, uint16_t len, KW307Frame_t* out)
{
    const uint8_t* p = data;
    if (len != 27) return -1;

    out->frame_index = dec_u16be(p);  p += 2;
    out->human_flag  = *p++;
    /* Wire layout: 2 moving-tracker blocks then 1 stationary block; all three
     * share the same 8-byte tracker format, so parse_tracker handles all. */
    p = parse_tracker(p, &out->moving[0]);
    p = parse_tracker(p, &out->moving[1]);
    p = parse_tracker(p, &out->stationary[0]);
    return 0;
}

/* ================================================================
   Decode - ACK frame (CMD 0xAC, LEN=2)
   Returns STATUS byte, or -1 if not a valid ACK.
   ================================================================ */
int kw307_dec_ack(uint8_t cmd, const uint8_t* data, uint16_t len,
                  uint8_t* acked_cmd_out)
{
    if (cmd != KW307_CMD_ACK) return -1;
    if (len  != 2)            return -1;
    if (acked_cmd_out) *acked_cmd_out = data[0];
    return (int)data[1];   /* STATUS */
}
