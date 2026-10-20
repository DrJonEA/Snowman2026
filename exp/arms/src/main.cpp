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

#include "Arm.h"
#include "ArmWave.h"
#include "ArmHug.h"
#include "HeadNod.h"
#include "HeadShake.h"


#define LED_LEN 7

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
    xNeopixels.fill( colours[0] );
	xNeopixels.show();

	Arm xLeft(&servoC, true);
	Arm xRight(&servoD, false);

	sleep_ms(3000);

	Head xHead(&servoB, &servoA);
	ArmWave xWaveLeft(&xLeft, 3);
	ArmWave xWaveRight(&xRight, 3);
	ArmHug xHugAnim(&xLeft, &xRight, 4);
	HeadNod xNodAnim(&xHead, 3);
	HeadShake xShakeAnim(&xHead, 3);

	Anim *anims[] = { &xWaveLeft, &xWaveRight, &xHugAnim, &xNodAnim, &xShakeAnim };
	const char *names[] = { "Wave left", "Wave right", "Hug", "Nod", "Shake" };
	size_t colourIndex = 0;

	for (;;){
		for (size_t i = 0; i < sizeof(anims) / sizeof(anims[0]); i++){
			xNeopixels.fill(colours[colourIndex]);
			xNeopixels.show();
			colourIndex = (colourIndex + 1) % COLOUR_COUNT;
			printf("%s\n", names[i]);
			anims[i]->start();
			while (anims[i]->isRunning()){
				anims[i]->poll();
				sleep_ms(10);
			}
			sleep_ms(1000);
		}
	}

#if 0
	Head xHead(&servoB, &servoA);
	HeadNod xNod(&xHead, 3, true);
	HeadShake xShake(&xHead, 3, true);
	xShake.start();
	for (;;){
		xShake.poll();
		sleep_ms(10);
	}
	xNod.start();
	for (;;){
		xNod.poll();
		sleep_ms(10);
	}

	ArmHug xHug(&xLeft, &xRight, 4, true);
	xHug.start();
	for (;;){
		xHug.poll();
		sleep_ms(10);
	}

	//ArmWave xWave(&xLeft, 3, true);
	ArmWave xWave(&xRight, 4, true);
	xWave.start();
	for (;;){
		xWave.poll();
		sleep_ms(10);
	}
#endif
	float x, y, z;
	for (;;){
		float r;
		for (int i=-1; i < 2; i ++){
			r = M_PI / (i * 2.0);
			if (i ==0){
				r = 0.0;
			}
			xLeft.moveRad(
							r,
							1000,
							ease_sin,
							true);
			xRight.moveRad(
							r,
							1000,
							ease_sin,
							true);
			for (int j=0; j < 20; j++){
				xLeft.getPos(x, y, z);
				printf("Left %.2f at %.2f, %.2f, %.2f\n", r, x, y, z);
				xRight.getPos(x, y, z);
				printf("Right %.2f at %.2f, %.2f, %.2f\n", r, x, y, z);
				sleep_ms(3000/20);
			}
		}
	}





    for (;;){
    	for (int i=0; i < COLOUR_COUNT; i++){
    		xNeopixels.fill( colours[i] );
    		xNeopixels.show();

    		switch (i%3){
    		case 0:
    			servo_time_to_deg(&servoD, 90.0, 1000000, ease_sin, NULL); //Right
    			servo_time_to_deg(&servoC, 90.0, 1000000, ease_sin, NULL); //LEFT
    			break;
    		case 1:
    			//Back
    			servo_time_to_deg(&servoD, 60.0, 1000000, ease_sin, NULL);
    			servo_time_to_deg(&servoC, 120.0, 1000000, ease_sin, NULL);
    			break;
    		default:
    			//Forward
    			servo_time_to_deg(&servoD, 150.0, 1000000, ease_sin, NULL);
    			servo_time_to_deg(&servoC, 25.0, 1000000, ease_sin, NULL);
    		}
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
