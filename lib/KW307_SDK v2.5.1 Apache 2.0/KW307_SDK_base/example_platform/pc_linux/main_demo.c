/**
 * KW307 SDK - Linux console demo
 * File: example_platform/pc_linux/main_demo.c
 *
 * Minimal host app: open the device, verify the module answers, apply a preset,
 * then print presence transitions. Start from this file for your own program.
 *
 * Build (from the SDK root):
 *   cc -std=c99 -O2 -Wall -I core -I example_platform/pc_linux \
 *      core/KW307_io.c core/KW307_api.c core/KW307_cmd.c \
 *      core/KW307_protocol.c core/KW307_uart.c \
 *      example_platform/pc_linux/port_linux.c \
 *      example_platform/pc_linux/main_demo.c -o kw307_demo
 *
 * Run:
 *   ./kw307_demo                 # device from user_config.h
 *   ./kw307_demo /dev/serial0    # or name one
 */

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1        /* nanosleep / clock_gettime visibility */
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "KW307_api.h"                /* pulls in KW307_io.h + KW307_protocol.h */
#include "kw307_example_linux.h"

/* Application state, set in the callback and read in main(). `volatile` because
 * the callback runs from inside the SDK's receive path. */
static volatile int      g_present     = 0;
static volatile int      g_prev        = -1;   /* -1 = nothing printed yet */
static volatile unsigned g_dist_cm     = 0;
static volatile int      g_angle_tenth = 0;
static volatile unsigned g_mag         = 0;
static volatile unsigned g_fc          = 0;

/* A moving slot is Valid at 1 in the simple reporting form, which is how the
 * module boots. Switch the module with kw307_set_tracker_state_advanced_mode(true)
 * and it becomes 3, so change this to match. The stationary slot is Valid at 1 in
 * both forms. Never test state != 0, and never decide on mag: mag is signal
 * strength, not a presence flag. */
#define MOVING_VALID  1u

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}

static void sleep_ms(long ms)
{
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

/* Fires on every radar frame. Keep it short — assign and return. Do NOT call
 * kw307_* from in here: the blocking calls would re-enter the receive path. */
static void on_radar(const KW307Frame_t* f)
{
    g_present = f->human_flag ? 1 : 0;
    g_fc      = f->frame_index;

    if (f->moving[0].state == MOVING_VALID) {
        g_dist_cm     = f->moving[0].distance_cm;
        g_angle_tenth = f->moving[0].angle_tenth;
        g_mag         = f->moving[0].mag;
    }
}

int main(int argc, char** argv)
{
    const char* dev = (argc > 1) ? argv[1] : NULL;
    double t0;

    printf("KW307 SDK Linux demo\n");

    if (dev) {
        kw307_example_set_port(dev, 0);          /* baud stays user_config.h's */
        printf("  device: %s\n", dev);
    } else {
        char devs[16][KW307_EXAMPLE_PORT_NAME_MAX];
        int  n = kw307_example_list_ports(devs, 16), i;
        printf("  device: from user_config.h  (serial devices present:%s",
               n ? "" : " none");
        for (i = 0; i < n; i++) printf(" %s", devs[i]);
        printf(")\n");
    }

    if (kw307_init() != KW307_OK) {
        fprintf(stderr, "  open failed — check the device path, and that you are\n"
                        "  in the dialout group:  sudo usermod -aG dialout $USER\n");
        return 1;
    }

    /* A device that opens proves only that the node exists: an unpowered or
     * miswired module opens just as happily. CMD 0x01 is answered by every
     * firmware, so asking for the version is the real liveness check. */
    {
        uint8_t maj = 0, min_ = 0, pat = 0;
        if (kw307_cmd_read_fw_version(500, &maj, &min_, &pat) != KW307_OK) {
            fprintf(stderr, "  no firmware answer — check power, the TX/RX\n"
                            "  crossover and the baud rate.\n");
            kw307_close();
            return 1;
        }
        printf("  module firmware v%u.%u.%u\n",
               (unsigned)maj, (unsigned)min_, (unsigned)pat);
    }
    if (kw307_check_fw_version(300) == 1)
        printf("  note: firmware older than this SDK's paired baseline\n");

    /* An office-style preset. See example_platform/port_template/main.c for the
     * other scenes (outdoor / desk / urinal / hand-dryer). */
    kw307_set_output_default();
    kw307_set_gain(5);                            /* +12 dB          */
    kw307_set_detect_range(50, 1000);             /* 0.5 ~ 10 m, cm  */
    kw307_set_detect_fov(120, 8);                 /* WHOLE degrees   */
    kw307_set_motion_sensitivity(2, 0);
    kw307_set_stationary_sensitivity(1);
    kw307_set_flag_hold_time(11);                 /* seconds         */

    kw307_on_frame(on_radar);
    printf("  streaming — Ctrl-C to exit\n\n");

    t0 = now_s();
    for (;;) {
        if (!kw307_port_is_open()) {         /* adapter unplugged */
            fprintf(stderr, "port lost\n");
            break;
        }
        kw307_update();                      /* fires on_radar */

        if (g_present != g_prev) {           /* print transitions, not every frame */
            g_prev = g_present;
            if (g_present)
                printf("[%6.1fs] PRESENT  dist=%4u cm  angle=%+5.1f deg  mag=%4u   (fc=%u)\n",
                       now_s() - t0, g_dist_cm, g_angle_tenth / 10.0, g_mag, g_fc);
            else
                printf("[%6.1fs] EMPTY                                            (fc=%u)\n",
                       now_s() - t0, g_fc);
            fflush(stdout);
        }

        /* The module streams ~20 frames a second into a finite driver buffer, so
         * the pump has to stay well above that. 10 ms is ~100 Hz. */
        sleep_ms(10);
    }

    kw307_close();
    return 0;
}
