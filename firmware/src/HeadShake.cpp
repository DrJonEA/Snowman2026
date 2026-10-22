/*
 * HeadShake.cpp
 */

#include "HeadShake.h"

#define SHAKE_RAD        0.4f
#define SHAKE_START_MS   300
#define SHAKE_SWING_MS   400
#define SHAKE_SETTLE_MS  400

HeadShake::HeadShake(Head *head, uint shakes, bool loop) : Anim(loop), pHead(head) {
	addStep([this]() { pan(SHAKE_RAD, SHAKE_START_MS); }, SHAKE_START_MS);
	for (uint i = 0; i < shakes; i++) {
		addStep([this]() { pan(-SHAKE_RAD, SHAKE_SWING_MS); }, SHAKE_SWING_MS);
		addStep([this]() { pan(SHAKE_RAD, SHAKE_SWING_MS); }, SHAKE_SWING_MS);
	}
	addStep([this]() { pHead->moveRad(0.0f, 0.0f, SHAKE_SETTLE_MS, ease_sin, true); }, SHAKE_SETTLE_MS);
}

HeadShake::~HeadShake() {
}

void HeadShake::pan(float rad, uint ms) {
	pHead->movePanRad(rad, ms, ease_sin, true);
}
