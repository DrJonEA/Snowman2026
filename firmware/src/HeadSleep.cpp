/*
 * HeadSleep.cpp
 */

#include "HeadSleep.h"

#define SLEEP_PAN_RAD 0.2f

HeadSleep::HeadSleep(Head *head, Arm *left, Arm *right, bool loop)
		: Anim(loop), pHead(head), pLeft(left), pRight(right) {
	// Start upright and settle the arms.
	addStep([this]() {
		droop(0.0f, 0.0f, 600);
		arms(ArmRest, 800);
	}, 1200);
	// Slow droop, then jerk awake.
	addStep([this]() { droop(0.0f, 0.5f, 1800); }, 2000);
	addStep([this]() { droop(0.0f, 0.0f, 250); }, 700);
	// Droop again, deeper, and a smaller jerk.
	addStep([this]() { droop(0.1f, 0.7f, 2200); }, 2400);
	addStep([this]() { droop(0.0f, 0.2f, 300); }, 600);
	// Final slow slump into the sleep pose; arms relax.
	addStep([this]() {
		droop(SLEEP_PAN_RAD, 1.0f, 3000);
		arms(ArmRest, 3000);
	}, 3500);
}

HeadSleep::~HeadSleep() {
}

void HeadSleep::droop(float pan, float tiltFraction, uint ms) {
	pHead->moveRad(pan, pHead->getTiltDownRad() * tiltFraction, ms, ease_sin, true);
}

void HeadSleep::arms(ArmPos pos, uint ms) {
	pLeft->movePosName(pos, ms, ease_sin);
	pRight->movePosName(pos, ms, ease_sin);
}
