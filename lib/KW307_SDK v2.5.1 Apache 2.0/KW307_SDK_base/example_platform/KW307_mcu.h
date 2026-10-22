/**
 * KW307 SDK - MCU Recipe Layer
 * For single-threaded MCU/Arduino main-loop customers.
 *
 * Quick start:
 *
 *   #include "KW307_mcu.h"
 *   int main(void) {
 *       kw307_init();
 *       // Scene preset goes here - see example_platform/<port>/main.c
 *       // for the switch{} block that applies a curated configuration.
 *       kw307_on_frame(on_radar);            // MCU auto-streams frames at power-on
 *       while (1) kw307_update();
 *   }
 *
 * This header is a thin convenience wrapper: it just pulls in KW307_api.h,
 * so MCU customers can write a single semantically-clear include line. The
 * public surface is split across two headers:
 *   - KW307_io.h   runtime / IO  (init, update, on_frame, read_blocking,
 *                                 KW307Err_t)
 *   - KW307_api.h  feature / config (set_gain, factory_reset, set_parameter, ...)
 *                                 — transitively pulls KW307_io.h.
 * Including KW307_mcu.h or KW307_api.h both yield the full surface.
 */

#ifndef KW307_MCU_H
#define KW307_MCU_H

#include "KW307_api.h"

#endif /* KW307_MCU_H */
