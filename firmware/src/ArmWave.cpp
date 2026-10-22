/*
 * ArmWave.cpp
 *
 *  Created on: 19 Oct 2026
 *      Author: jondurrant
 */

#include "ArmWave.h"
#include <math.h>

#define WAVE_LIFT_MS   1000
#define WAVE_SWING_MS  400
#define WAVE_REST_MS   1000

ArmWave::ArmWave(Arm *a, uint waves, bool loop) : Anim(loop), pArm(a) {
	addStep([this]() { move(ArmForward, WAVE_LIFT_MS); }, WAVE_LIFT_MS);
	for (uint i = 0; i < waves; i++) {
		addStep([this]() { move(ArmRest, WAVE_SWING_MS); }, WAVE_SWING_MS);
		addStep([this]() { move(ArmForward, WAVE_SWING_MS); }, WAVE_SWING_MS);
	}
	addStep([this]() { move(ArmRest, WAVE_REST_MS); }, WAVE_REST_MS);
}

ArmWave::~ArmWave() {
}

void ArmWave::move(ArmPos pos, uint ms) {
	pArm->movePosName(pos, ms, ease_sin);
}

