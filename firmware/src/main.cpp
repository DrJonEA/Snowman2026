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
#include "HeadLook.h"
#include "HeadSleep.h"
#include "Dance.h"
#include "WakeUp.h"
#include "Star.h"
#include "StarChase.h"
#include "StarHeartbeat.h"

#include "hardware/uart.h"
#include "KW307_api.h"


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

/* Application-level state — set from the callback, read in main().
 * `volatile` because the callback may run from inside the UART RX path. */
static volatile uint8_t  g_human_flag      = 0;
static volatile uint16_t g_target_dist_cm  = 0;   /* nearest moving-target distance, cm */


/* Fires on every radar frame. Keep short — heavy work belongs in main(). */
static void on_radar(const KW307Frame_t* f){
    /* The frame carries the whole result: f->human_flag, both moving
       targets (f->moving[0], f->moving[1]) and the stationary target
       (f->stationary[0]). This demo uses only the nearest moving target;
       read the others the same way. */

    g_human_flag     = f->human_flag ? 1u : 0u;
    g_target_dist_cm = f->moving[0].distance_cm;  /* 0 when no target */

    printf("Frame %u Human %u \n", f->frame_index, f->human_flag);
    printf("Stationary: %ucm %.1fdeg  mag: %u  State: %u Counter: %u\n",
    		f->stationary[0].distance_cm,
			(float)f->stationary[0].angle_tenth/ 10.0,
			f->stationary[0].mag,
			f->stationary[0].state,
			f->stationary[0].counter
			);
    for (int i=0; i < 2; i++){
    	 printf("Moving(%d): %ucm %.1fdeg  mag: %u  State: %u Counter: %u\n",
    			 i,
				f->moving[0].distance_cm,
				(float)f->moving[0].angle_tenth/ 10.0,
				f->moving[0].mag,
				f->moving[0].state,
				f->moving[0].counter
				);
    }
}

int setupKW307() {
	if (kw307_init() != KW307_OK) {
		printf("KW307 Init Failed\n");
		sleep_ms(2000);
		return -1;
	}
	if (kw307_check_fw_version(300) != 0){
		printf("KW307 Version Test Failed\n");
		sleep_ms(2000);
		return -1;
	}

    kw307_set_gain               (5);              /* +12 dB */
	kw307_set_detect_range       (50, 1000);       /* 0.5~10 m */
	kw307_set_detect_fov         (120, 8);        /* 120 deg cone, shrink 8 deg/m */
	kw307_set_motion_sensitivity    (2, 0);
	kw307_set_stationary_sensitivity(1);
	kw307_set_moving_tracker_cnt(32, 8);    /* life 32f, show 8 */
	kw307_set_stationary_tracker_cnt(64, 8);    /* life 64f, show 8 */
	kw307_set_flag_hold_time (11);          /* presence flag hold 11 s */
	kw307_set_max_stationary_time(1, 20);          /* still cap on, 20 s */
	kw307_set_force_stationary   (true, 150, 40);  /* on, 150 cm, 40 deg */
	kw307_set_advanced_clutter_rejection(true,  300, 20);

	kw307_on_frame(on_radar);
	return 0;
}


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

	setupKW307();

    Star xStar(pio1, 0, GP_WS2812B, 30);
    StarChase xChase(&xStar, colours[0], 1000, true);
    StarHeartbeat xHeart(&xStar, colours[4], 1000, true);
    xChase.start();
    xHeart.start();

	Arm xLeft(&servoC, true);
	Arm xRight(&servoD, false);

	sleep_ms(3000);

	Head xHead(&servoB, &servoA);
	ArmWave xWaveLeft(&xLeft, 3);
	ArmWave xWaveRight(&xRight, 3);
	ArmHug xHugAnim(&xLeft, &xRight, 4);
	HeadNod xNodAnim(&xHead, 3);
	HeadShake xShakeAnim(&xHead, 3);
	HeadLook xLookAnim(&xHead);
	HeadSleep xSleepAnim(&xHead, &xLeft, &xRight);
	Dance xDanceAnim(&xHead, &xLeft, &xRight);
	WakeUp xWakeAnim(&xHead, &xLeft, &xRight);

	Anim *anims[] = { &xWaveLeft, &xWaveRight, &xHugAnim, &xNodAnim, &xShakeAnim, &xLookAnim, &xDanceAnim, &xSleepAnim, &xWakeAnim };
	const char *names[] = { "Wave left", "Wave right", "Hug", "Nod", "Shake", "Look", "Dance", "Sleep", "Wake up" };
	size_t colourIndex = 0;

	for (;;){
		for (size_t i = 0; i < sizeof(anims) / sizeof(anims[0]); i++){
			xChase.setColour(colours[colourIndex]);
			xHeart.setColour(colours[(colourIndex + 4) % COLOUR_COUNT]);
			colourIndex = (colourIndex + 1) % COLOUR_COUNT;
			printf("%s\n", names[i]);
			anims[i]->start();
			while (anims[i]->isRunning()){
				anims[i]->poll();
				xChase.poll();
				xHeart.poll();
				sleep_ms(10);
			}
			for (int t = 0; t < 100; t++){
				xChase.poll();
				xHeart.poll();
				sleep_ms(10);
			}
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
    		xStar.fill( colours[i] );
    		xStar.show();

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
