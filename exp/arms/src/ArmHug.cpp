/*
 * ArmHug.cpp
 */

#include "ArmHug.h"

#define HUG_CLOSE_MS    1200
#define HUG_WIGGLE_MS   250
#define HUG_RELEASE_MS  1200
#define HUG_HOLD_MS     500
#define HUG_LOOSE       0.8f

ArmHug::ArmHug(Arm *left, Arm *right, uint wiggles, bool loop)
		: Anim(loop), pLeft(left), pRight(right) {
	addStep([this]() { moveBoth(ArmForward, HUG_CLOSE_MS); }, HUG_CLOSE_MS + HUG_HOLD_MS);
	for (uint i = 0; i < wiggles; i++) {
		addStep([this]() { wiggleBoth(HUG_LOOSE, HUG_WIGGLE_MS); }, HUG_WIGGLE_MS);
		addStep([this]() { wiggleBoth(1.0f, HUG_WIGGLE_MS); }, HUG_WIGGLE_MS);
	}
	addStep([this]() { moveBoth(ArmRest, HUG_RELEASE_MS); }, HUG_RELEASE_MS);
}

ArmHug::~ArmHug() {
}

void ArmHug::moveBoth(ArmPos pos, uint ms) {
	pLeft->movePosName(pos, ms, ease_sin);
	pRight->movePosName(pos, ms, ease_sin);
}

void ArmHug::wiggleBoth(float fraction, uint ms) {
	pLeft->moveRad(pLeft->getForwardRad() * fraction, ms, ease_sin, true);
	pRight->moveRad(pRight->getForwardRad() * fraction, ms, ease_sin, true);
}
