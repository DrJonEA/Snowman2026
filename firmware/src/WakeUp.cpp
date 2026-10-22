/*
 * WakeUp.cpp
 */

#include "WakeUp.h"

WakeUp::WakeUp(Head *head, Arm *left, Arm *right, bool loop)
		: Anim(loop), pHead(head), pLeft(left), pRight(right) {
	// Gentle stir: head lifts a little and turns slightly.
	addStep([this]() { pose(0.15f, -0.6f, 0.0f, 0.0f, 1500); }, 1700);
	// Slow rise, a soft look to one side as if getting bearings.
	addStep([this]() { pose(-0.15f, -0.2f, 0.1f, 0.1f, 1500); }, 1700);
	addStep([this]() { pose(0.1f, 0.1f, 0.2f, 0.2f, 1500); }, 1700);
	// Come to full attention: head level and centred, arms at rest.
	addStep([this]() { pose(0.0f, 0.0f, 0.0f, 0.0f, 1800); }, 2000);
}

WakeUp::~WakeUp() {
}

void WakeUp::pose(float pan, float tilt, float armL, float armR, uint ms) {
	pLeft->moveRad(pLeft->getForwardRad() * armL, ms, ease_sin, true);
	pRight->moveRad(pRight->getForwardRad() * armR, ms, ease_sin, true);
	float t = tilt >= 0.0f ? pHead->getTiltUpRad() * tilt
			: pHead->getTiltDownRad() * -tilt;
	pHead->moveRad(pan, t, ms, ease_sin, true);
}
