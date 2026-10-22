/**
 * PDX-License-Identifier: Apache-2.0
 * Copyright 2026 Keywave Technology Limited
 */


/**
 * KW307 SDK - Feature API - Implementation
 * FW: 2.7.5
 */

#include <stddef.h>     /* NULL */
#include "KW307_api.h"
#include "KW307_cmd.h"
#include "KW307_uart.h"
#include "KW307_port.h"


/* Forward declaration of the SDK's singleton UART accessor (implemented in
 * KW307_io.c — kept out of any header so the singleton stays an internal
 * concept of the api/cmd/io layer triad). */
extern KW307Uart_t* kw307_default_uart(void);

/* SDK-side cache of the last filter 6-tuple (CMD 0x17 sets BOTH bands
 * at once: {fK,fNd,fLPF, sK,sNd,sLPF}). The two Level setters
 * (kw307_set_motion_sensitivity / kw307_set_stationary_sensitivity) each touch
 * only their own band, so we keep the other band's bytes here and re-send the
 * full 6 — no read-modify of the wire needed. Seeded with the MCU boot defaults:
 * moving band {khpf 4, Nd 0, LPF 128}; stationary band {khpf 7, Nd 2, LPF 8}.  */
static uint8_t s_dop[6] = { 4u, 0u, 128u, 7u, 2u, 8u };

/* ================================================================
   Sec.1  System CMDs (0x01 ~ 0x03 + 0x07 + 0x09)
   ================================================================ */

int kw307_radar_init(void)
{
    /* CMD 0x07 — re-init the radar. ~130 ms blocking on the MCU side. */
    uint8_t v = 0x01u;
    return kw307_send_cmd(0x07u, &v, 1);
}

int kw307_factory_reset(void)
{
    return kw307_cmd_factory_reset(kw307_default_uart()) == 0
           ? KW307_OK : KW307_ERR_PORT;
}

int kw307_save_settings(void)
{
    return kw307_cmd_save_settings(kw307_default_uart()) == 0
           ? KW307_OK : KW307_ERR_PORT;
}

int kw307_clear_state(void)
{
    /* CMD 0x09 — drops the current targets and presence flag. No settings and
     * no flash are touched. */
    return kw307_cmd_clear_state(kw307_default_uart()) == 0
           ? KW307_OK : KW307_ERR_PORT;
}

int kw307_check_fw_version(int timeout_ms)
{
    uint8_t maj = 0, min_ = 0, patch = 0;
    int ret = kw307_cmd_read_fw_version(timeout_ms, &maj, &min_, &patch);
    if (ret != KW307_OK) return ret;   /* transport error — propagate */

    /* Compatible = MCU firmware >= the paired baseline (KW307_SDK_FW_*).
     * Compared field-by-field, highest priority first (MAJOR, then MINOR, then
     * PATCH). The first field that differs decides and short-circuits: a newer
     * field means a newer firmware, so the lower fields need not be checked. */
    if (maj  != (uint8_t)KW307_SDK_FW_MAJOR)
        return (maj  > (uint8_t)KW307_SDK_FW_MAJOR) ? 0 : 1;   /* MAJOR decides */
    if (min_ != (uint8_t)KW307_SDK_FW_MINOR)
        return (min_ > (uint8_t)KW307_SDK_FW_MINOR) ? 0 : 1;   /* MINOR decides */
    if (patch != (uint8_t)KW307_SDK_FW_PATCH)
        return (patch > (uint8_t)KW307_SDK_FW_PATCH) ? 0 : 1;  /* PATCH decides */
    return 0;       /* exactly the baseline version */
}

/* ================================================================
   Sec.2  Output mode (CMD 0x11)
   ================================================================ */

int kw307_set_output_default(void)
{
    return kw307_cmd_output_mode(kw307_default_uart(), KW307_OUTPUT_FRAME);
}

int kw307_set_output_off(void)
{
    return kw307_cmd_output_mode(kw307_default_uart(), KW307_OUTPUT_OFF);
}

/* ================================================================
   Sec.3  Device config — power / gain / range / FOV / sensitivity / filters / trackers / hold
   ================================================================ */

int kw307_set_power_down(bool down)
{
    /* down=true  -> mode 0 (Power Down — deepest sleep);
     * down=false -> mode 1 (Normal). */
    return kw307_cmd_power_mode(kw307_default_uart(), down ? 0u : 1u) == 0
           ? KW307_OK : KW307_ERR_PORT;
}

int kw307_set_gain(uint8_t index)
{
    if (index > 8) return KW307_ERR_PARAM;
    return kw307_cmd_gain(kw307_default_uart(), index) == 0
           ? KW307_OK : KW307_ERR_PORT;
}

int kw307_set_detect_range(uint16_t min_cm, uint16_t max_cm)
{
    /* min must be strictly less than max, or no valid detection window exists.
     * Upper cap 3270 cm keeps the mm value (×10) inside int16 range. */
    if (min_cm >= max_cm) return KW307_ERR_PARAM;
    if (max_cm > 3270u)   return KW307_ERR_PARAM;
    /* API uses cm; wire is mm -> ×10 before sending. */
    return kw307_cmd_detect_range(kw307_default_uart(),
                                  (uint16_t)(min_cm * 10u),
                                  (uint16_t)(max_cm * 10u)) == 0
           ? KW307_OK : KW307_ERR_PORT;
}

int kw307_set_detect_fov(int16_t fov_deg, uint8_t shrink_rate_deg)
{
    /* fov_deg: 1~180 (physical antenna front hemisphere); SDK pre-multiplies
     * x10 into 0.1 deg on the wire. shrink_rate_deg upper limit 25 keeps the
     * x10 result inside uint8_t (25*10 = 250). */
    if (fov_deg <= 0 || fov_deg > 180) return KW307_ERR_PARAM;
    if (shrink_rate_deg > 25)          return KW307_ERR_PARAM;
    return kw307_cmd_detect_fov(kw307_default_uart(),
                                 (int16_t)(fov_deg * 10),
                                 (uint8_t)(shrink_rate_deg * 10)) == 0
           ? KW307_OK : KW307_ERR_PORT;
}

int kw307_set_mag_threshold(int16_t baseline_moving, int16_t baseline_stationary)
{
    /* Both baselines are int16 on the wire, but customers should always pass
     * non-negative values (0~32767). Catching <0 here
     * keeps the API contract self-consistent and lets the MCU trust the wire. */
    if (baseline_moving < 0 || baseline_stationary < 0) return KW307_ERR_PARAM;
    return kw307_cmd_signal_threshold(kw307_default_uart(), baseline_moving, baseline_stationary) == 0
           ? KW307_OK : KW307_ERR_PORT;
}

/* ---- High-level "Level" sensitivity presets ------------------------------
 * Each level bundles the relevant filter + gate parameters into one preset,
 * so callers pick a single 0~9 level instead of tuning individual params. */

static int dop_send(void)   /* re-send the cached 6-tuple over CMD 0x17 */
{
    return kw307_cmd_doppler_filter(kw307_default_uart(),
               s_dop[0], s_dop[1], s_dop[2], s_dop[3], s_dop[4], s_dop[5]) == 0
           ? KW307_OK : KW307_ERR_PORT;
}

int kw307_set_motion_sensitivity(uint8_t level, uint8_t byp_spd)
{
    /* Monotonic moving-target sensitivity, L0 (most) -> L9 (least): each level
     * maps to a moving-band HPF cutoff + a Fast-band speed gate (table below).
     * byp_spd != 0 forces the moving HPF to its lowest cutoff, the speed
     * threshold to 1 and pass_initial to 4, and turns the speed-sum gate off,
     * so the tracker reacts to any motion regardless of speed (e.g. a hand
     * dryer). */
    static const struct { uint8_t khpf; int16_t spd_th; uint8_t pts; } T[10] = {
        { 4u,  250, 4u },  /* L0  most sensitive */
        { 3u,  250, 4u },  /* L1 */
        { 3u,  250, 8u },  /* L2 */
        { 3u,  500, 4u },  /* L3 */
        { 2u,  500, 4u },  /* L4 */
        { 2u,  500, 8u },  /* L5 */
        { 2u, 1000, 4u },  /* L6 */
        { 1u,  500, 8u },  /* L7 */
        { 1u, 1000, 4u },  /* L8 */
        { 1u, 1000, 8u },  /* L9  least sensitive */
    };
    if (level > 9u) return KW307_ERR_PARAM;
    int rc;

    /* byp_spd -> lowest HPF cutoff (khpf 7 / Nd 3); otherwise the level's
     * cutoff with Nd = 0. Moving band only. */
    if (byp_spd) { s_dop[0] = 7u; s_dop[1] = 3u; }
    else         { s_dop[0] = T[level].khpf; s_dop[1] = 0u; }
    rc = dop_send();                              if (rc) return rc;

    /* Fast-band speed gate — values clamped to int16. spd_sum = spd_th * pts. */
    {
        int16_t spd_th = byp_spd ? (int16_t)1 : T[level].spd_th;  /* byp_spd -> min threshold 1 (old L9) */
        uint8_t pts    = byp_spd ? 4u : T[level].pts;             /* byp_spd -> pass_initial 4 (old L9) */
        int32_t lim = (int32_t)spd_th * 2;                  if (lim > 32767) lim = 32767;
        int32_t sum = byp_spd ? 0 : (int32_t)spd_th * pts;  if (sum > 32767) sum = 32767;
        rc = kw307_set_parameter(0x26u, (uint8_t)((uint16_t)spd_th >> 8),
                                        (uint8_t)((uint16_t)spd_th & 0xFFu), 2u);  if (rc) return rc;
        rc = kw307_set_parameter(0x1Au, (uint8_t)(((uint16_t)lim) >> 8),
                                        (uint8_t)(((uint16_t)lim) & 0xFFu), 2u);   if (rc) return rc;
        rc = kw307_set_parameter(0x31u, 0u, pts, 1u);                             if (rc) return rc;
        rc = kw307_set_parameter(0x28u, (uint8_t)(((uint16_t)sum) >> 8),
                                        (uint8_t)(((uint16_t)sum) & 0xFFu), 2u);
    }
    return rc;
}

int kw307_set_stationary_sensitivity(uint8_t level)
{
    /* L0 (most sensitive) ~ L9 (least). Maps the level to slow-band
     * HPF khpf/Nd + slow LPF, plus a +6 dB baseline boost on L7~L9. */
    static const struct { uint8_t khpf, nd, lpf, boost; } T[10] = {
        { 7u, 3u,  8u, 0u },  /* L0  most sensitive */
        { 7u, 2u,  8u, 0u },  /* L1 */
        { 7u, 1u,  8u, 0u },  /* L2 */
        { 7u, 0u,  8u, 0u },  /* L3 */
        { 6u, 0u,  8u, 0u },  /* L4 */
        { 5u, 0u,  8u, 0u },  /* L5 */
        { 4u, 0u, 16u, 0u },  /* L6 */
        { 4u, 0u, 16u, 1u },  /* L7 */
        { 3u, 0u, 32u, 1u },  /* L8 */
        { 2u, 0u, 64u, 1u },  /* L9  least sensitive */
    };
    if (level > 9u) return KW307_ERR_PARAM;
    int16_t  base_n = T[level].boost ? 700 : 350;
    int16_t  base_s = T[level].boost ? 200 : 100;
    int rc;

    s_dop[3] = T[level].khpf;  s_dop[4] = T[level].nd;  s_dop[5] = T[level].lpf; /* slow HPF+LPF only */
    rc = dop_send();                                                            if (rc) return rc;
    rc = kw307_set_parameter(0x24u, (uint8_t)((uint16_t)base_n >> 8),
                                    (uint8_t)((uint16_t)base_n & 0xFFu), 2u);   if (rc) return rc; /* baseline_N */
    return kw307_set_parameter(0x25u, (uint8_t)((uint16_t)base_s >> 8),
                                      (uint8_t)((uint16_t)base_s & 0xFFu), 2u);  /* baseline_S */
}

int kw307_set_moving_tracker_cnt(uint8_t cnt_max, uint8_t cnt_show_th)
{
    /* show_th must be <= cnt_max (equal allowed). Beyond that the target never
     * reaches Valid and always reads Empty. MCU re-validates on the wire. */
    if (cnt_show_th > cnt_max) return KW307_ERR_PARAM;
    return kw307_cmd_moving_tracker_cnt(kw307_default_uart(), cnt_max, cnt_show_th) == 0
           ? KW307_OK : KW307_ERR_PORT;
}

int kw307_set_stationary_tracker_cnt(uint8_t cnt_max, uint8_t cnt_show_th)
{
    if (cnt_show_th > cnt_max) return KW307_ERR_PARAM;
    return kw307_cmd_stationary_tracker_cnt(kw307_default_uart(), cnt_max, cnt_show_th) == 0
           ? KW307_OK : KW307_ERR_PORT;
}

int kw307_set_force_stationary(bool on, int16_t distance_cm, int16_t fov_deg)
{
    /* distance_cm: MCU x10 -> mm. Cap at uint16 mm representable range. */
    if (distance_cm < 0 || distance_cm > 3276) return KW307_ERR_PARAM;
    /* fov_deg: 0~180 (physical antenna front hemisphere); SDK pre-multiplies
     * x10 into 0.1 deg on the wire. */
    if (fov_deg     < 0 || fov_deg     > 180 ) return KW307_ERR_PARAM;

    int rc;
    rc = kw307_set_parameter(0x5Au, 0u, on ? 1u : 0u, 1u);
    if (rc) return rc;
    rc = kw307_set_parameter(0x5Bu,
                              (uint8_t)((uint16_t)distance_cm >> 8),
                              (uint8_t)((uint16_t)distance_cm & 0xFFu), 2u);
    if (rc) return rc;
    uint16_t fov_tenth = (uint16_t)(fov_deg * 10);
    return kw307_set_parameter(0x5Cu,
                                (uint8_t)(fov_tenth >> 8),
                                (uint8_t)(fov_tenth & 0xFFu), 2u);
}

int kw307_set_max_stationary_time(uint8_t enable, uint16_t seconds)
{
    if (enable > 1u)     return KW307_ERR_PARAM;
    if (seconds > 3276u) return KW307_ERR_PARAM;  /* 3276 * 20 = 65520 < uint16 max */
    /* PID 0x36 is a uint16 FRAME count.  */
    uint16_t frames = (uint16_t)(seconds * 20u);
    int rc = kw307_set_parameter(0x36u,
                                  (uint8_t)(frames >> 8),
                                  (uint8_t)(frames & 0xFFu), 2u);
    if (rc) return rc;
    /* PID 0x40: 1 = enable the cap, 0 = disable. */
    return kw307_set_parameter(0x40u, 0u, enable, 1u);
}

/* Defaults: spd_th=300 (0..1000), dev_th=20 cm (0..50); higher = reject more. */
int kw307_set_advanced_clutter_rejection(bool en, int16_t spd_th, int16_t dev_th)
{
    int rc;
    if (spd_th < 0 || dev_th < 0) return KW307_ERR_PARAM;
    rc = kw307_set_parameter(0x59u, 0u, en ? 1u : 0u, 1u);                       if (rc) return rc; /* enable */
    rc = kw307_set_parameter(0x5Du, (uint8_t)((uint16_t)spd_th >> 8),
                                    (uint8_t)((uint16_t)spd_th & 0xFFu), 2u);     if (rc) return rc; /* speed_th */
    return kw307_set_parameter(0x5Eu, (uint8_t)((uint16_t)dev_th >> 8),
                                      (uint8_t)((uint16_t)dev_th & 0xFFu), 2u);                      /* dev_th (cm) */
}

int kw307_set_flag_hold_time(uint16_t seconds)
{
    /* Presence-flag hold (debounce): how long human_flag stays asserted after
     * the last detection. Radar @ 20 Hz, so the SDK pre-multiplies seconds into
     * the frame counter before sending.
     * Boot default: 6 s (120 frames). 0 s is floored to 1 frame so the flag is
     * always held for at least one output frame. */
    if (seconds > 3276u) return KW307_ERR_PARAM;  /* 3276 * 20 = 65520 < uint16 max */
    uint16_t frames = (uint16_t)(seconds * 20u);
    if (frames == 0u) frames = 1u;                /* 0 s still transmits 1 frame */
    return kw307_set_parameter(0x37u,
                                (uint8_t)(frames >> 8),
                                (uint8_t)(frames & 0xFFu), 2u);
}

int kw307_set_tracker_state_advanced_mode(bool advanced)
{
    /* PID 0x6B — status form of the target slots: simple (default) or advanced. */
    return kw307_set_parameter(0x6Bu, 0u, advanced ? 1u : 0u, 1u);
}

int kw307_set_gpio_human_flag(bool on)
{
    /* PID 0xFF — presence output pin: 1 = follow the presence flag (default),
     * 0 = hold the pin low. Re-enabling applies the presence state the radar is
     * already holding, since detection never stopped. Not persisted. */
    return kw307_set_parameter(0xFFu, 0u, on ? 1u : 0u, 1u);
}
