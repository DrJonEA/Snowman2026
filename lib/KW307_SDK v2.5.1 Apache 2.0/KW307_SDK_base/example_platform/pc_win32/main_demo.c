/******************************************************************************
 * KW307 SDK - Windows PC Minimal Example
 *
 * Console app that opens the COM port set in user_config.h, applies the OFFICE
 * scene preset, and prints presence events as they arrive.
 *
 * Output format (default: 20Hz):
 *     [Frame index=1234] PRESENT  dist=  45 cm  angle= +1.2 deg  mag= 832
 *     [Frame index=1265] EMPTY                                           
 *     [Frame index=1271] PRESENT  dist=  52 cm  angle= -0.5 deg  mag= 910
 *
 * Prints on:
 *   - ENTER : transition no-person -> person detected
 *   - EXIT  : transition person -> no-person
 *
 * Build (add these .c to your build target):
 *   KW307_SDK/core/KW307_io.c
 *   KW307_SDK/core/KW307_api.c
 *   KW307_SDK/core/KW307_cmd.c
 *   KW307_SDK/core/KW307_protocol.c
 *   KW307_SDK/core/KW307_uart.c
 *   KW307_SDK/example_platform/pc_win32/port_win32.c
 *   KW307_SDK/example_platform/pc_win32/main_demo.c        (this file)
 *
 * Include path:
 *   KW307_SDK/core/
 *   KW307_SDK/example_platform/pc_win32/    (user_config.h)
 *
 * Customise:
 *   user_config.h    - COM port name / baud rate
 *****************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <windows.h>          /* GetTickCount, Sleep — Windows-only deps */
#include "KW307_api.h"        /* check_fw_version, typed setters, set_parameter */
#include "KW307_io.h"         /* init / update / on_frame */
#include "KW307_protocol.h"   /* KW307Frame_t */


/* Scene preset selector — change DEMO_SCENE to retarget the curated
 * configuration applied before the main loop. */
#define DEMO_SCENE_OUTDOOR     0   /* FOV 120, 12 m, hold  5 s              */
#define DEMO_SCENE_OFFICE      1   /* FOV 120, 10 m, hold 11 s              */
#define DEMO_SCENE_DESK        2   /* FOV 120,  2 m, hold 11 s              */
#define DEMO_SCENE_URINAL      3   /* FOV  60, 1.5 m, hold 1 s              */
#define DEMO_SCENE_HAND_DRYER  4   /* FOV  80,  1 m, fast response (byp_spd) */
#define DEMO_SCENE             DEMO_SCENE_OFFICE


/* Edge-triggered state — set in the callback, read in main(). */
static volatile int     g_present_now      = 0;   /* current human_flag */
static volatile int     g_present_last     = -1;  /* previous (-1 = never seen) */
static volatile uint16_t g_dist_cm         = 0;
static volatile int16_t  g_angle_tenth     = 0;
static volatile uint16_t g_mag             = 0;
static volatile uint16_t g_frame_idx       = 0;

static DWORD g_t0_ms = 0;


/* Fires from inside kw307_update(). Keep short — heavy work goes to main(). */
/* A moving slot is Valid at 1 in the simple reporting form — how the module boots.
   Switch the module with kw307_set_tracker_state_advanced_mode(true) and a moving
   slot is Valid at 3 instead, so change this to 3 as well. The stationary slot is
   Valid at 1 in
   both forms. Never test state != 0, and never decide on mag either: mag is signal
   strength, not a presence flag. */
#define MOVING_VALID  1u

static void on_radar(const KW307Frame_t* f)
{
    /* The frame carries the whole result: f->human_flag, both moving
       targets (f->moving[0], f->moving[1]) and the stationary target
       (f->stationary[0]). This demo uses only the nearest moving target;
       read the others the same way — a slot counts only when it is Valid
       (MOVING_VALID above). */

    g_present_now  = f->human_flag ? 1 : 0;
    g_frame_idx    = f->frame_index;

    if (f->moving[0].state == MOVING_VALID) {   /* a real target */
        g_dist_cm     = f->moving[0].distance_cm;
        g_angle_tenth = f->moving[0].angle_tenth;
        g_mag         = f->moving[0].mag;
    } else {                                /* nothing in this slot */
        g_dist_cm     = 0;
        g_angle_tenth = 0;
        g_mag         = 0;
    }
}


static void print_edge(int present)
{
    double t_s = (double)(GetTickCount() - g_t0_ms) / 1000.0;
    if (present) {
        printf("[%5.1fs] PRESENT  dist=%4u cm  angle=%+5.1f deg  mag=%4u   (fc=%u)\n",
               t_s,
               (unsigned)g_dist_cm,
               (double)g_angle_tenth / 10.0,
               (unsigned)g_mag,
               (unsigned)g_frame_idx);
    } else {
        printf("[%5.1fs] EMPTY%47s(fc=%u)\n",
               t_s, "", (unsigned)g_frame_idx);
    }
    fflush(stdout);
}


int main(void)
{
    printf("KW307 SDK PC demo (Win32) — opening COM port from user_config.h\n");

    /* (a) Open the SDK (reads UART settings from user_config.h). */
    int rc = kw307_init();
    if (rc != KW307_OK) {
        printf("!!! kw307_init() failed: %d\n", rc);
        printf("    Check user_config.h (KW307_DEFAULT_PORT_NAME) and that the\n");
        printf("    COM port is not held by another application.\n");
        return 1;
    }

    /* (b) Optional but recommended: verify MCU FW version matches this SDK's
     *     pairing. 0=match, 1=mismatch, <0=link error. */
    rc = kw307_check_fw_version(300);
    if (rc < 0)       printf("!!! FW check transport error: %d (UART link down?)\n", rc);
    else if (rc == 1) printf("!!! FW version mismatch — proceeding, but behaviour undefined.\n");
    else              printf("    FW version matches SDK pairing (v%u.%u.%u).\n",
                             KW307_SDK_FW_MAJOR, KW307_SDK_FW_MINOR, KW307_SDK_FW_PATCH);

    /* (c) Apply scene preset — switch{} below applies a curated radar configuration. Edit
     *     DEMO_SCENE at top of file to retarget.
     *     The raw kw307_set_parameter() writes are firmware-version specific. */
    kw307_set_output_default();
    switch (DEMO_SCENE) {
    case DEMO_SCENE_OUTDOOR:
        kw307_set_gain               (5);              /* +12 dB */
        kw307_set_detect_range       (50, 1200);       /* 0.5~12 m */
        kw307_set_detect_fov         (120, 8);        /* 120 deg cone, shrink 8 deg/m */
        kw307_set_motion_sensitivity    (6, 0);
        kw307_set_stationary_sensitivity(3);
        kw307_set_moving_tracker_cnt(32, 8);    /* life 32f, show 8 */
        kw307_set_stationary_tracker_cnt(64, 8);    /* life 64f, show 8 */
        kw307_set_flag_hold_time (5);           /* presence flag hold 5 s */
        kw307_set_max_stationary_time(1, 5);           /* still cap on, 5 s */
        kw307_set_force_stationary   (false, 150, 40); /* off */
        kw307_set_advanced_clutter_rejection(false, 300, 20);
        break;
    case DEMO_SCENE_OFFICE:
        kw307_set_gain               (5);              /* +12 dB */
        kw307_set_detect_range       (50, 1000);       /* 0.5~10 m */
        kw307_set_detect_fov         (120, 8);        /* 120 deg cone, shrink 8 deg/m */
        kw307_set_motion_sensitivity    (2, 0);
        kw307_set_stationary_sensitivity(1);
        kw307_set_moving_tracker_cnt(32, 8);    /* life 32f, show 8 */
        kw307_set_stationary_tracker_cnt(64, 8);    /* life 64f, show 8 */
        kw307_set_flag_hold_time (11);          /* presence flag hold 11 s */
        kw307_set_max_stationary_time(1, 20);          /* still cap on, 20 s */
        kw307_set_force_stationary   (true, 150, 40);  /* on, 150 cm, 40 deg */
        kw307_set_advanced_clutter_rejection(true,  300, 20);
        break;
    case DEMO_SCENE_DESK:
        kw307_set_gain               (3);              /* +6 dB */
        kw307_set_detect_range       (50, 200);        /* 0.5~2 m */
        kw307_set_detect_fov         (120, 8);        /* 120 deg cone, shrink 8 deg/m */
        kw307_set_motion_sensitivity    (0, 0);
        kw307_set_stationary_sensitivity(0);
        kw307_set_moving_tracker_cnt(32, 8);    /* life 32f, show 8 */
        kw307_set_stationary_tracker_cnt(64, 8);    /* life 64f, show 8 */
        kw307_set_flag_hold_time (11);          /* presence flag hold 11 s */
        kw307_set_max_stationary_time(1, 120);         /* still cap on, 120 s */
        kw307_set_force_stationary   (true, 150, 40);  /* on, 150 cm, 40 deg */
        kw307_set_advanced_clutter_rejection(true,  300, 20);
        break;
    case DEMO_SCENE_URINAL:
        kw307_set_gain               (3);              /* +6 dB */
        kw307_set_detect_range       (10, 150);        /* 0.1~1.5 m */
        kw307_set_detect_fov         (60, 5);          /* 60 deg cone, -5 deg/step */
        kw307_set_motion_sensitivity    (1, 0);
        kw307_set_stationary_sensitivity(1);
        kw307_set_moving_tracker_cnt(32, 8);    /* life 32f, show 8 */
        kw307_set_stationary_tracker_cnt(64, 8);    /* life 64f, show 8 */
        kw307_set_flag_hold_time (1);           /* presence flag hold 1 s */
        kw307_set_max_stationary_time(1, 60);          /* still cap on, 60 s */
        kw307_set_force_stationary   (true, 150, 40);  /* on, 150 cm, 40 deg */
        kw307_set_advanced_clutter_rejection(false, 300, 20);
        break;
    case DEMO_SCENE_HAND_DRYER:
        kw307_set_gain               (5);              /* +12 dB */
        kw307_set_detect_range       (10, 100);        /* 0.1~1 m */
        kw307_set_detect_fov         (80, 5);          /* 80 deg cone, -5 deg/step */
        kw307_set_motion_sensitivity    (0, 1);   /* L0 + speed-gate bypass (react to any motion) */
        kw307_set_stationary_sensitivity(1);
        kw307_set_mag_threshold   (200, 5000);      /* floor: moving 200 / still 5000 (muted) */
        kw307_set_moving_tracker_cnt(20, 2);    /* life 20f, show 2 */
        kw307_set_stationary_tracker_cnt(64, 2);    /* life 64f, show 2 */
        kw307_set_flag_hold_time (1);           /* presence flag hold 1 s */
        kw307_set_max_stationary_time(1, 10);          /* still cap on, 10 s */
        kw307_set_force_stationary   (false, 150, 40); /* off */
        kw307_set_advanced_clutter_rejection(false, 300, 20);
        break;
    }

    /* (d) Register the per-frame callback. */
    kw307_on_frame(on_radar);

    g_t0_ms = GetTickCount();
    printf("    Streaming radar frames — press Ctrl+C to exit.\n\n");

    /* (e) Main loop: drain RX → fire callback → print presence transitions.
     *
     * Idle-only sleep: with non-blocking COMMTIMEOUTS, kw307_update() returns
     * immediately whether bytes were available or not, so an unconditional
     * Sleep here would throttle RX below the MCU's stream rate (20 Hz x
     * ~34 byte frame = ~680 byte/s). Only yield when the pump drained
     * nothing — when data is flowing, fall straight through. */
    for (;;) {
        int n = kw307_update();

        if (g_present_now != g_present_last) {
            print_edge(g_present_now);
            g_present_last = g_present_now;
        }

        if (n <= 0) Sleep(1);   /* idle yield so the loop doesn't peg a CPU core */
    }

    /* unreachable — Ctrl+C exits the process */
    /* kw307_close(); */
    /* return 0; */
}
