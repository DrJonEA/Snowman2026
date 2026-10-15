/*
 * main.cpp
 *
 *  Created on: 7 Sept 2026
 *      Author: jondurrant
 */

#include "pico/stdlib.h"
#include <stdio.h>

#include "ServoHead.h"

#include "PicoLed.hpp"

extern "C" {
#include "servo.h"
}


#define LED_LEN 14

PicoLed::PicoLedController xNeopixels = PicoLed::addLeds<PicoLed::WS2812B>(
					pio1, 0,
					GP_WS2812B,
					LED_LEN,
					PicoLed::FORMAT_GRB);

#define COLOUR_COUNT 8
PicoLed::Color colours[COLOUR_COUNT] = {
		PicoLed::RGB(228,3 , 3),
		PicoLed::RGB(255, 140, 0),
		PicoLed::RGB(255, 237, 0),
		PicoLed::RGB(0, 128, 38),
		PicoLed::RGB(0, 76, 255),
		PicoLed::RGB(115, 41, 130),
		PicoLed::RGB(97, 57, 21),
		PicoLed::RGB(116, 215, 238)
};


Servo servoA = {
    .gpio               = PWM1A,
    .period_usec        = 20000u,
    .duty_min_usec      = 500,
    .duty_max_usec      = 2500u,
};

Servo servoB = {
    .gpio               = PWM1B,
    .period_usec        = 20000u,
    .duty_min_usec      = 500,
    .duty_max_usec      = 2500u,
};

Servo servoC = {
    .gpio               = PWM2A,
    .period_usec        = 20000u,
    .duty_min_usec      = 700,
    .duty_max_usec      = 2300u,
};

Servo servoD = {
    .gpio               = PWM2B,
    .period_usec        = 20000u,
    .duty_min_usec      = 700,
    .duty_max_usec      = 2300u,
};

Servo servoE = {
    .gpio               = PWM3A,
    .period_usec        = 20000u,
    .duty_min_usec      = 700,
    .duty_max_usec      = 2300u,
};

#define POS_COUNT 8
float pos[POS_COUNT] = {
		90.0,
		45.0,
		0.0,
		45.0,
		90.0,
		135.0,
		180.0,
		135.0
};

int main()
{
    stdio_init_all();
    sleep_ms(2000);
    printf("GO\n");

    servo_init(&servoA);
    servo_init(&servoB);
    servo_init(&servoC);
    servo_init(&servoD);
    servo_init(&servoE);
    servo_set_deg(&servoA, 90.0);
    servo_set_deg(&servoB, 90.0);
    servo_set_deg(&servoC, 90.0);
    servo_set_deg(&servoD, 90.0);
    servo_set_deg(&servoE, 90.0);


    stdio_init_all();
    sleep_ms(2000);
    printf("GO\n");

    xNeopixels.setBrightness(30);

    for (;;){
    	for (int i=0; i < COLOUR_COUNT; i++){
    		xNeopixels.fill( colours[i] );
    		xNeopixels.show();
    		//servo_set_deg(&servoA, pos[i]);
    		//servo_set_deg(&servoB, pos[i]);
    		servo_time_to_deg(&servoA, pos[i], 1000000, ease_in_expo, NULL);
    		servo_time_to_deg(&servoB, pos[i], 1000000, ease_in_expo, NULL);
    		servo_time_to_deg(&servoC, pos[i], 1000000, ease_in_expo, NULL);
    		servo_time_to_deg(&servoD, pos[i], 1000000, ease_in_expo, NULL);
    		printf("Move to %.1f\n", pos[i]);
    		sleep_ms(500*6);
    	}
    }

    for (;;){

    	for (int i=0; i < POS_COUNT; i++){
			servo_set_deg(&servoA, pos[i]);
			servo_set_deg(&servoB, pos[i]);
			sleep_ms(5000);
    	}

    	for (int i=0; i < POS_COUNT; i++){
    		servo_time_to_deg(&servoA, pos[i], 1000000, NULL, NULL);
    		servo_time_to_deg(&servoB, pos[i], 1000000, ease_in_expo, NULL);
			sleep_ms(5000);
    	}

	}
}
