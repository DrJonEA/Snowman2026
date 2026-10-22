/**
 * PDX-License-Identifier: Apache-2.0
 * Copyright 2026 Keywave Technology Limited
 */

/**
 * KW307 SDK - High-Level Customer API
 * FW: 2.7.5
 *
 * Include this header to *change* radar behaviour (gain, range, FOV,
 * sensitivity, trackers, hold-time, ...). It also pulls in KW307_io.h, so you
 * get init / update / on_frame / read_blocking as well.
 *
 * If you only need to *receive* frames on the radar's saved defaults, include
 * KW307_io.h alone.
 *
 *   Sec.1  System         firmware version / factory reset / save / radar re-init /
 *                         clear detection state
 *   Sec.2  Output mode    no need to change in the base SDK
 *   Sec.3  Device config  power / gain / range / FOV / sensitivity / ...
 *
 * Typical startup:
 *   kw307_init();
 *   kw307_check_fw_version(300);
 *   // apply your settings here (see example_platform/<port>/main.c)
 *   kw307_on_frame(on_radar);
 *   while (1) kw307_update();
 *
 * Every setter returns KW307_OK on success, or a negative KW307_ERR_* code
 * (see KW307_io.h). Arguments out of range return KW307_ERR_PARAM.
 */

#ifndef KW307_API_H
#define KW307_API_H

#include <stdint.h>
#include <stdbool.h>
#include "KW307_io.h"
#include "KW307_protocol.h"
#include "KW307_cmd.h"      /* raw access: kw307_send_cmd / read_cmd / set_parameter */

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
   Sec.1  System
   ================================================================ */

/* KW307 SDK version — the version of THIS SDK source, independent of the paired
 * MCU firmware version below. Bump on any API or behaviour change. */
#define KW307_SDK_VERSION_MAJOR  2u
#define KW307_SDK_VERSION_MINOR  5u
#define KW307_SDK_VERSION_PATCH  1u

/* MCU firmware this SDK release is paired with (release B.2) — the baseline
 * kw307_check_fw_version() measures against. Anything NEWER passes; a module older
 * than this is reported incompatible, so raising it declares every earlier module
 * out of date. Keep it equal to the firmware the release actually ships with. */
#define KW307_SDK_FW_MAJOR  2u
#define KW307_SDK_FW_MINOR  7u
#define KW307_SDK_FW_PATCH  5u

/** Re-initialise the radar (~130 ms, blocking). Re-applies the current
 *  configuration. Call after a setting that needs an init to take effect. */
int kw307_radar_init(void);

/** Clear saved settings and reload firmware defaults. */
int kw307_factory_reset(void);

/** Persist the current settings to flash; restored automatically on next
 *  power-up. Don't call in a loop — each call uses a flash write cycle. */
int kw307_save_settings(void);

/**
 * Drop the targets the module is holding and reset the presence flag, so detection
 * starts over; settings are kept. Targets take a moment to reappear.
 * Needs firmware 2.7.2 or newer.
 */
int kw307_clear_state(void);

/**
 * Check the connected MCU firmware is compatible with this SDK.
 * Compatible = firmware version >= the paired baseline
 * (KW307_SDK_FW_MAJOR.MINOR.PATCH). Compared field-by-field, highest priority
 * first (MAJOR, then MINOR, then PATCH); the first differing field decides and
 * short-circuits, so any newer firmware passes.
 *   0  : compatible (baseline or newer)
 *   1  : incompatible — firmware older than the paired baseline
 *  <0  : link error (no response / UART down)
 */
int kw307_check_fw_version(int timeout_ms);


/* ================================================================
   Sec.2  Output mode
   ================================================================ */
/**
 *   kw307_set_output_default()  (re)select the default frame — presence +
 *                               2 moving + 1 still target (consume with
 *                               kw307_on_frame).
 *   kw307_set_output_off()      stop the periodic stream (the radar keeps
 *                               running); works for whichever frame is active.
 */
int kw307_set_output_default(void);
int kw307_set_output_off(void);

/* ----------------------------------------------------------------
   The default frame (wire CMD 0xF0) — layout and how to read it

   >>> Display a slot only when its state is Valid. This holds in BOTH
   >>> reporting forms; do not use "state is non-zero" and do not use `mag`.

   kw307_on_frame() hands you one decoded KW307Frame_t, ~20 per second:

     +---------------+------------------------------------------------+
     | frame_index   | running frame counter                          |
     +---------------+------------------------------------------------+
     | human_flag    | 1 = someone present / 0 = nobody.              |
     |               | Already debounced (kw307_set_flag_hold_time),  |
     |               | so for a plain presence output THIS FLAG ALONE |
     |               | is enough — the slots are only needed when you |
     |               | want target positions.                         |
     +---------------+------------------------------------------------+
     | moving[0]     | distance_cm  angle_tenth  mag  state  counter  |
     | moving[1]     | distance_cm  angle_tenth  mag  state  counter  |
     | stationary[0] | distance_cm  angle_tenth  mag  state  counter  |
     +---------------+------------------------------------------------+
                                                 ^^^^^
                          which value is Valid (= the only one to display):

                        |  simple form       |  advanced form
                        |  (the default)     |  (for evaluation)
     -------------------+--------------------+--------------------------
      moving[0..1]      |  Valid  = 1        |  Valid  = 3
      stationary[0]     |  Valid  = 1        |  Valid  = 1
      empty slot        |  Empty  = 0        |  Empty  = 0
                        |  (all other fields |
                        |   zeroed as well)  |
      never display     |  --                |  moving      1 Checking
                        |                    |              2 Birth
                        |                    |    (candidates, not confirmed)
                        |                    |  stationary  2 Hidden
                        |                    |    (same spot as a moving
                        |                    |     target — displaying it
                        |                    |     shows one person twice)

     Switch forms with kw307_set_tracker_state_advanced_mode(false / true).
     `state != 0` happens to equal Valid in the simple form only, so testing for
     Valid is the one habit that stays correct if the form is ever switched.
     Never test `mag != 0`: `mag` is signal strength, not a presence flag.
   ---------------------------------------------------------------- */

/* ================================================================
   Sec.3  Device config
   ================================================================ */

/** Deep-sleep Power Down.
 *    down = true  -> RF powered down, lowest current; frame output stops
 *                    (a UART byte from the host wakes it).
 *    down = false -> Normal (resume sensing).
 *  Boot default: false. */
int kw307_set_power_down(bool down);

/** RF gain. index 0~8: -3 / 0 / +3 / +6 / +9 / +12 / +15 / +18 / +21 dB.
 *  Boot default: 5 (+12 dB). */
int kw307_set_gain(uint8_t index);

/** Min + max detection distance in cm.
 *    min_cm : near gate (e.g. 50 = 0.5 m).  max_cm : far gate (e.g. 1000 = 10 m).
 *  Boot defaults: 50 / 1000. min_cm must be < max_cm. */
int kw307_set_detect_range(uint16_t min_cm, uint16_t max_cm);

/** Detection cone — how wide the radar sees in front.
 *    fov_deg         : cone width in whole degrees (1~180). Larger = picks up
 *                      people off to the sides. Boot default: 120.
 *    shrink_rate_deg : how much the cone narrows per 100cm (0~25 deg/100cm);
 *                      higher = tighter cone at long range. Boot default: 8.
 *                      You can use this to define a rectangle-like area.
 *  Tuning: false triggers from people passing by -> reduce fov_deg;
 *  missing off-centre targets at long range -> reduce shrink_rate_deg. */
int kw307_set_detect_fov(int16_t fov_deg, uint8_t shrink_rate_deg);

/** Mag threshold — detection magnitude floor for moving and stationary targets.
 *  Higher = less sensitive. Range per band 0~32767.
 *    baseline_moving     : moving-target floor      (default 350)
 *    baseline_stationary : stationary-target floor  (default 100) */
int kw307_set_mag_threshold(int16_t baseline_moving, int16_t baseline_stationary);

/** Motion (moving-target) sensitivity.
 *    level   : 0 (most sensitive) ~ 9 (least sensitive) — a monotonic curated
 *              preset of the moving-band HPF + Fast-band speed gate.
 *    byp_spd : 0 = normal; non-zero switches the moving HPF to its lowest cutoff
 *              and turns the speed-sum gate off, so the tracker reacts to any
 *              motion regardless of accumulated speed (e.g. a hand dryer that
 *              must fire the instant a hand appears).
 *  Affects the moving band only. level > 9 returns KW307_ERR_PARAM. */
int kw307_set_motion_sensitivity(uint8_t level, uint8_t byp_spd);

/** Stationary (still-target) sensitivity as a single level.
 *    0~9 : most-sensitive (0, catches breathing-rate motion) -> least (9);
 *          levels 7~9 also raise the detection floor (+6 dB).
 *  Affects the stationary band only. level > 9 returns KW307_ERR_PARAM. */
int kw307_set_stationary_sensitivity(uint8_t level);

/** Tune the moving-target tracker counters.
 *    cnt_max     : hold time of a moving target.
 *                  how long a target survives without re-detection,
 *                  in frames (20 fps). Default 32.
 *    cnt_show_th : response time of a moving target.
 *                  The target becomes Valid once cnt >= cnt_show_th.
 *                  Default 8. Must be <= cnt_max. */
int kw307_set_moving_tracker_cnt(uint8_t cnt_max, uint8_t cnt_show_th);

/** Tune the stationary-target tracker counters.
 *  Same meanings as the moving tracker;
 * Defaults: cnt_max 64, cnt_show_th 8. */
int kw307_set_stationary_tracker_cnt(uint8_t cnt_max, uint8_t cnt_show_th);

/** Keep the presence signal asserted while a still target stands inside a
 *  forward window (distance x field-of-view), so brief detection dropouts
 *  don't flicker presence while a person sits very still.
 *
 *    on          enable (true) / disable (false).
 *    distance_cm window depth in cm (0~3276); a still target must be closer.
 *    fov_deg     window width in whole degrees (0~180), edge to edge
 *                (e.g. 60 = +/- 30 deg around directly-ahead).
 *  Boot defaults: on = true, distance_cm = 150, fov_deg = 40.
 *
 *  Tuning: presence flickers while a person sits still -> widen the window;
 *  presence lingers after they leave -> narrow it. */
int kw307_set_force_stationary(bool on, int16_t distance_cm, int16_t fov_deg);

/** Maximum time a still target may sit before the radar releases it.
 *    enable  : 1 = enforce the cap, 0 = never auto-release on time.
 *    seconds : lifetime, 0~3276 s. Higher for offices/lounges, lower for
 *              fast-cycle scenes (urinals, hand dryers).
 *  Boot default: enabled, 20 s.
 *  Tuning: presence stays "occupied" too long after the person left -> shorten;
 *  person dropped while still sitting -> lengthen, or set enable = 0. */
int kw307_set_max_stationary_time(uint8_t enable, uint16_t seconds);

/** Reject repetitive non-human motion (AC, machinery, ...) so it is not
 *  reported as presence.
 *    en     : true = on (default), false = report every moving target.
 *    spd_th : motion threshold — higher = reject more. Range 0..1000, default 300.
 *    dev_th : allowed drift in cm — higher = reject more. Range 0..50, default 20. */
int kw307_set_advanced_clutter_rejection(bool en, int16_t spd_th, int16_t dev_th);

/**
 * Tracker-state reporting form.
 *   false : simple (default) — `state` is 0 Empty / 1 Valid; an Empty slot has its
 *           other fields zeroed.
 *   true  : advanced — `state` also reports the in-between stages, and a moving
 *           target is Valid at 3 instead of 1.
 * Reporting form only; detection is unchanged. Effective from the next frame.
 * Firmware older than 2.7.0 always reports the advanced form.
 */
int kw307_set_tracker_state_advanced_mode(bool advanced);

/** Presence-flag hold time (debounce): how long the presence flag stays
 *  asserted after the last detection, so it doesn't flicker between frames.
 *  seconds : 0~3276 s (held as seconds*20 frames internally; 0 s is floored to
 *            1 frame, so the flag always holds for at least one output frame).
 *  Boot default: 6 s (120 frames). seconds > 3276 returns KW307_ERR_PARAM.*/
int kw307_set_flag_hold_time(uint16_t seconds);

/**
 * Presence output pin on / off.
 *   true  : the pin follows the presence flag (default).
 *   false : the pin is held low.
 * Switching it back on applies the presence state the module already holds — it
 * does not restart detection, so a person already in front of the sensor drives
 * the pin high on the next frame. Not saved by kw307_save_settings().
 */
int kw307_set_gpio_human_flag(bool on);



#ifdef __cplusplus
}
#endif

#endif /* KW307_API_H */
