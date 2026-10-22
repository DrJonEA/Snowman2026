/**
 * KW307 SDK - PC Win32 User Configuration
 * File: example_platform/pc_win32/user_config.h
 *
 * *** Edit this file to match your hardware. ***
 *
 * Usage:
 *   kw307_init();   // automatically reads KW307_DEFAULT_PORT_NAME / BAUD_RATE
 *
 * To override at runtime, pass non-NULL/non-zero arguments to the lower-level
 * kw307_port_open() — the values here are only the fallback defaults.
 */

#ifndef KW307_USER_CONFIG_PC_WIN32_H
#define KW307_USER_CONFIG_PC_WIN32_H

/* ----------------------------------------------------------------
   COM port name
   COM1~COM9 : "COM3"
   COM10+    : "\\\\.\\COM10"     (the port_win32.c implementation
                                   adds the \\.\ prefix automatically,
                                   so plain "COM10" also works.)
   ---------------------------------------------------------------- */
#define KW307_DEFAULT_PORT_NAME   "COM3"

/* ----------------------------------------------------------------
   Baud rate (must match KW307 MCU firmware setting, typically 115200)
   ---------------------------------------------------------------- */
#define KW307_DEFAULT_BAUD_RATE   115200

#endif /* KW307_USER_CONFIG_PC_WIN32_H */
