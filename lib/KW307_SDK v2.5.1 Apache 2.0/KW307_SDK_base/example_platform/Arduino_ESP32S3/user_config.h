/**
 * KW307 SDK - Arduino User Configuration
 * File: Arduino_ESP32S3/user_config.h
 *
 * * Edit this file in your sketch folder to match your wiring. *
 *
 * Default baud rate is 115200 (matches KW307 UART0 factory default).
 * The HardwareSerial instance and pin assignment are selected in your
 * .ino file via kw307_port_arduino_bind(Serial1, RX, TX)
 * (declared in port_arduino.h).
 */

#ifndef KW307_USER_CONFIG_ARDUINO_H
#define KW307_USER_CONFIG_ARDUINO_H

/* Baud rate must match the KW307 firmware setting (default 115200). */
#define KW307_ARDUINO_UART_BAUD   115200UL

#endif /* KW307_USER_CONFIG_ARDUINO_H */
