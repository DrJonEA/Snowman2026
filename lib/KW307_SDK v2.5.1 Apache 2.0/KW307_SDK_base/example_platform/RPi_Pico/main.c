/**
 * KW307 SDK - Raspberry Pi Pico demo
 * File: example_platform/RPi_Pico/main.c
 *
 * The onboard LED follows presence, and every transition is printed over USB
 * serial. Start from this file for your own application.
 *
 * Build + flash: see README.md in this folder.
 */

#include <stdio.h>

#include "pico/stdlib.h"

#include "KW307_api.h"        /* pulls in KW307_io.h + KW307_protocol.h */
#include "user_config.h"

/* Pico / Pico 2 onboard LED. On a Pico W the LED hangs off the wireless chip
 * instead, so there is no PICO_DEFAULT_LED_PIN — use the cyw43 driver there, or
 * just watch the USB output. */
#ifdef PICO_DEFAULT_LED_PIN
#define LED_PIN PICO_DEFAULT_LED_PIN
#endif

/* Set by the frame callback, read by the main loop. `volatile` because the
 * callback runs from inside the SDK's receive path. */
static volatile bool     g_present = false;
static volatile uint16_t g_dist_cm = 0;
static volatile int16_t  g_angle_tenth = 0;
static volatile uint16_t g_fc = 0;

/* A moving slot is Valid at 1 in the simple reporting form, which is how the
 * module boots. Switch it with kw307_set_tracker_state_advanced_mode(true) and a
 * moving slot becomes Valid at 3, so change this to match. The stationary slot
 * is Valid at 1 in both forms. Never test state != 0, and never decide on mag:
 * mag is signal strength, not a presence flag. */
#define MOVING_VALID  1u

/* Fires on every radar frame. Keep it short — assign and return. Do NOT call
 * kw307_* from in here: the blocking calls would re-enter the receive path. */
static void on_radar(const KW307Frame_t* f)
{
    g_present = (f->human_flag != 0);
    g_fc      = f->frame_index;

    if (f->moving[0].state == MOVING_VALID) {
        g_dist_cm     = f->moving[0].distance_cm;
        g_angle_tenth = f->moving[0].angle_tenth;
    }
}

int main(void)
{
    bool     prev = false;
    bool     first = true;
    uint8_t  maj = 0, min_ = 0, pat = 0;

    stdio_init_all();          /* USB serial — see CMakeLists.txt */

#ifdef LED_PIN
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);
#endif

    /* Give a host terminal a moment to attach before the first prints, or the
     * startup lines vanish into a port nobody is listening to yet. */
    sleep_ms(2000);
    printf("\nKW307 SDK Pico demo\n");

    if (kw307_init() != KW307_OK) {
        printf("  kw307_init failed — check user_config.h (UART + pins)\n");
        while (1) tight_loop_contents();
    }

    /* A UART that initialises proves nothing about the module: an unpowered or
     * miswired one looks identical. CMD 0x01 is answered by every firmware, so
     * asking for the version is the real liveness check. */
    if (kw307_cmd_read_fw_version(500, &maj, &min_, &pat) != KW307_OK) {
        printf("  no answer from the module. Check:\n"
               "   - 5V on VBUS (pin 40) and a common GND\n"
               "   - TX/RX crossed: GP%d -> module RXD, GP%d <- module TXD\n"
               "   - baud matches (user_config.h)\n",
               KW307_UART_TX_PIN, KW307_UART_RX_PIN);
        while (1) tight_loop_contents();
    }
    printf("  module firmware v%u.%u.%u\n",
           (unsigned)maj, (unsigned)min_, (unsigned)pat);
    if (kw307_check_fw_version(300) == 1)
        printf("  note: firmware older than this SDK's paired baseline\n");

    /* An office-style preset. See example_platform/port_template/main.c for the
     * other scenes (outdoor / desk / urinal / hand-dryer). */
    kw307_set_output_default();
    kw307_set_gain(5);                      /* +12 dB           */
    kw307_set_detect_range(50, 1000);       /* 0.5 ~ 10 m, cm   */
    kw307_set_detect_fov(120, 8);           /* WHOLE degrees    */
    kw307_set_motion_sensitivity(2, 0);
    kw307_set_stationary_sensitivity(1);
    kw307_set_flag_hold_time(11);           /* seconds          */

    kw307_on_frame(on_radar);
    printf("  streaming\n\n");

    while (true) {
        kw307_update();                     /* drains the ring, fires on_radar */

        if (first || g_present != prev) {
            first = false;
            prev  = g_present;
#ifdef LED_PIN
            gpio_put(LED_PIN, g_present ? 1 : 0);
#endif
            if (g_present)
                printf("PRESENT  dist=%4u cm  angle=%+5.1f deg   (fc=%u)\n",
                       (unsigned)g_dist_cm, g_angle_tenth / 10.0f,
                       (unsigned)g_fc);
            else
                printf("EMPTY                                    (fc=%u)\n",
                       (unsigned)g_fc);
        }

        /* The module streams ~20 frames a second into a 256-byte ring, so the
         * pump must stay well above that. 5 ms is ~200 Hz and leaves the core
         * almost entirely free for your own work. */
        sleep_ms(5);
    }
}
