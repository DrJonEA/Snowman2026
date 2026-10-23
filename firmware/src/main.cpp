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
#include "SnowmanBrain.h"

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

/* Application-level state — set from the callback, read in main().
 * `volatile` because the callback may run from inside the UART RX path. */
static volatile RadarTarget_t g_target     = {false, false, 0, 0};
static volatile uint32_t g_target_seq      = 0;   /* odd while g_target is being written */

static RadarTarget_t getTarget(){
	RadarTarget_t t;
	uint32_t s;
	do {
		s = g_target_seq;
		t.valid       = g_target.valid;
		t.moving      = g_target.moving;
		t.angle_tenth = g_target.angle_tenth;
		t.dist_cm     = g_target.dist_cm;
	} while ((s & 1u) || s != g_target_seq);
	return t;
}

/* Distance filter, run per radar frame: a median over the last few frames
 * throws out single-frame spikes (servo/supply noise), then a light exponential
 * average smooths what is left. Latency is about 2 frames, so it stays responsive.
 * History is cleared after a gap with no target. */
#define DIST_MEDIAN_LEN   5
#define DIST_EMA_ALPHA    0.5f
#define DIST_GAP_FRAMES   10

static uint16_t filterDistance(bool valid, uint16_t raw){
	static uint16_t hist[DIST_MEDIAN_LEN];
	static uint count = 0, next = 0, gap = 0;
	static float smooth = 0.0f;

	if (!valid || raw == 0) {
		if (++gap >= DIST_GAP_FRAMES) {
			count = 0;
			next = 0;
		}
		return raw;
	}
	gap = 0;
	hist[next] = raw;
	next = (next + 1) % DIST_MEDIAN_LEN;
	if (count < DIST_MEDIAN_LEN) {
		count++;
	}
	uint16_t s[DIST_MEDIAN_LEN];
	for (uint i = 0; i < count; i++) {
		s[i] = hist[i];
	}
	for (uint i = 1; i < count; i++) {
		for (uint j = i; j > 0 && s[j - 1] > s[j]; j--) {
			uint16_t t = s[j]; s[j] = s[j - 1]; s[j - 1] = t;
		}
	}
	float median = s[count / 2];
	smooth = (count == 1) ? median : smooth + DIST_EMA_ALPHA * (median - smooth);
	return (uint16_t)(smooth + 0.5f);
}

/* Fires on every radar frame. Keep short — heavy work belongs in main(). */
static void on_radar(const KW307Frame_t* f){
	/* Stationary target (state 1) wins; otherwise the nearest moving target
	   (state 3 in the advanced reporting form). */
	RadarTarget_t t = {false, false, 0, 0};
	if (f->stationary[0].state == 1) {
		t = {true, false, f->stationary[0].angle_tenth, f->stationary[0].distance_cm};
	} else {
		for (int i = 0; i < 2; i++) {
			const auto &mv = f->moving[i];
			if (mv.state == 1 && (!t.valid || (mv.distance_cm > 0 && mv.distance_cm < t.dist_cm))) {
				t = {true, true, mv.angle_tenth, mv.distance_cm};
			}
		}
	}
	t.dist_cm = filterDistance(t.valid, t.dist_cm);

	g_target_seq = g_target_seq + 1;
	g_target.valid = t.valid;
	g_target.moving = t.moving;
	g_target.angle_tenth = t.angle_tenth;
	g_target.dist_cm = t.dist_cm;
	g_target_seq = g_target_seq + 1;
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

    kw307_set_output_default();
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

	Head xHead(&servoA, &servoB);
	ArmWave xWaveLeft(&xLeft, 3);
	ArmWave xWaveRight(&xRight, 3);
	ArmHug xHugAnim(&xLeft, &xRight, 4);
	HeadNod xNodAnim(&xHead, 3);
	HeadShake xShakeAnim(&xHead, 3);
	HeadLook xLookAnim(&xHead);
	HeadSleep xSleepAnim(&xHead, &xLeft, &xRight);
	Dance xDanceAnim(&xHead, &xLeft, &xRight);
	WakeUp xWakeAnim(&xHead, &xLeft, &xRight);

	BrainActors actors = { &xHead, &xStar, &xChase, &xHeart,
			&xWakeAnim, &xLookAnim, &xWaveRight, &xDanceAnim, &xSleepAnim,
			&xNodAnim, &xShakeAnim, &xWaveLeft, &xWaveRight };
	Anim *anims[] = {
			&xWaveLeft, &xWaveRight, &xHugAnim, &xNodAnim, &xShakeAnim,
			&xLookAnim, &xDanceAnim, &xSleepAnim, &xWakeAnim, &xChase, &xHeart
	};
	SnowmanBrain xBrain(actors);
	xBrain.start();

	for (;;){
		RadarTarget_t tgt = getTarget();
		xBrain.updateSensor(tgt.valid, tgt.dist_cm);
		xBrain.updateTarget(tgt);
		xBrain.poll();
		for (Anim *anim : anims){
			anim->poll();
		}
		sleep_ms(2);  // short tick: the LED animations step every 20 ms
	}
}
