/**
 * KW307 SDK - Arduino / ESP32 HardwareSerial Port
 * File: Arduino_ESP32S3/port_arduino.h
 *
 * Implements KW307_port.h over Arduino HardwareSerial. Tested on
 * ESP32 / ESP32-S3. Should work on any board exposing a
 * HardwareSerial that implements Stream.
 *
 * USAGE (in your .ino):
 *
 *   #include "port_arduino.h"
 *
 *   void setup() {
 *       // Bind SDK to a HardwareSerial AND start it in one call.
 *       // ESP32: pick pins for RX / TX.
 *       kw307_port_arduino_begin(Serial1, 18, 17);
 *
 *       kw307_init();
 *       // Scene preset goes here - copy the switch{} from
 *       // example_platform/Nuvoton_M251/main.c (or another port).
 *       kw307_on_frame(on_radar);           // MCU auto-streams frames at power-on
 *       // ...
 *   }
 *
 * On other Arduino cores that don't accept RX/TX pins in Serial.begin(),
 * call Serial1.begin(115200) yourself and then kw307_port_arduino_bind(Serial1).
 */

#ifndef PORT_ARDUINO_H
#define PORT_ARDUINO_H

#include "KW307_port.h"

#ifdef __cplusplus
#include <HardwareSerial.h>

/**
 * Bind the SDK to an already-configured HardwareSerial.
 * Call BEFORE kw307_init(). Serial.begin(...) must be done by you.
 */
void kw307_port_arduino_bind(HardwareSerial& serial);

/**
 * Convenience helper: bind + begin in one call (ESP32 / ESP32-S3).
 * Uses baud = KW307_ARDUINO_UART_BAUD (see user_config.h).
 * rx_pin / tx_pin are passed to HardwareSerial::begin().
 */
void kw307_port_arduino_begin(HardwareSerial& serial, int rx_pin, int tx_pin);

#endif /* __cplusplus */

#endif /* PORT_ARDUINO_H */
