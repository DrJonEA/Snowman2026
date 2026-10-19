/*
 * AstroDroidConfig.h
 *
 *  Created on: 25 Jan 2025
 *      Author: jondurrant
 */

#ifndef EXP_BOARDLEDS_SRC_ASTRODROIDCONFIG_H_
#define EXP_BOARDLEDS_SRC_ASTRODROIDCONFIG_H_


//DEBUG
#define DEBUG_LED1 21
#define DEBUG_LED2 22

//RP2040Zero Pins
#define UART0_TX 0
#define UART0_RX 1

#define GP_STEP_DETECT 	3
#define GP_STEP 						2
#define GP_DIR    						4
#define GP_SLEEP    					6
#define GP_M0							26
#define GP_M1							27
#define GP_M2							28
#define GP_HALL						12

#define GP_LX16A_TX 8
#define GP_LX16A_RX 9

#define GP_WS2812B 10


#define ZERO_NEOPIXEL	16

#define PWM2B 5
#define PWM3B 7
#define PWM5B 11
#define PWM6B 13
#define PWM7B 15

#define PIOTX 4
#define PIORX 3

#define UART1_TX 4
#define UART1_RX 5


#endif /* EXP_BOARDLEDS_SRC_ASTRODROIDCONFIG_H_ */
