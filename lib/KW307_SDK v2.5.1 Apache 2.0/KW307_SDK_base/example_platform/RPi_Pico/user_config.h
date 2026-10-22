/**
 * KW307 SDK - Raspberry Pi Pico User Configuration
 * File: example_platform/RPi_Pico/user_config.h
 *
 * *** Edit this file to match your wiring. ***
 *
 * Applies to RP2040 (Pico / Pico W) and RP2350 (Pico 2) with pico-sdk.
 */

#ifndef KW307_USER_CONFIG_PICO_H
#define KW307_USER_CONFIG_PICO_H

/* ----------------------------------------------------------------
   Which UART, and which pins

   The Pico has two UARTs, and each can be routed to several pin pairs:
     uart0 : GP0/GP1 (default), GP12/GP13, GP16/GP17
     uart1 : GP4/GP5, GP8/GP9
   TX and RX must belong to the same UART instance.

   IMPORTANT: pick a UART the radar can have to itself. If you leave stdio on
   UART (see CMakeLists.txt) printf and the radar will fight over the same pins.
   ---------------------------------------------------------------- */
#define KW307_UART_ID        uart0
#define KW307_UART_TX_PIN    0        /* GP0 -> module RXD */
#define KW307_UART_RX_PIN    1        /* GP1 <- module TXD */

/* ----------------------------------------------------------------
   Baud rate — must match the KW307 module's firmware (typically 115200).
   ---------------------------------------------------------------- */
#define KW307_DEFAULT_BAUD_RATE   115200

/* ----------------------------------------------------------------
   RX ring buffer, filled by the UART interrupt and drained by
   kw307_update(). The module sends a 27-byte frame ~20x a second (about
   540 byte/s), so 256 bytes is roughly nine frames of slack — enough to
   cover a main loop that goes away for ~450 ms.

   MUST be a power of two: the ring index wraps with a mask, because
   Cortex-M0+ (RP2040) has no hardware divide instruction.
   ---------------------------------------------------------------- */
#define KW307_UART_RX_BUFSIZE   256u

#endif /* KW307_USER_CONFIG_PICO_H */
