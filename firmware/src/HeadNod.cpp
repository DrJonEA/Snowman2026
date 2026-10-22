/*
 * HeadNod.cpp
 */

#include "HeadNod.h"

#define NOD_DOWN_MS    350
#define NOD_UP_MS      350
#define NOD_SETTLE_MS  500

HeadNod::HeadNod(Head *head, uint nods, bool loop) : Anim(loop), pHead(head) {
	for (uint i = 0; i < nods; i++) {
		addStep([this]() { tilt(1.0f, NOD_DOWN_MS); }, NOD_DOWN_MS);
		addStep([this]() { tilt(0.0f, NOD_UP_MS); }, NOD_UP_MS);
	}
	addStep([this]() { pHead->moveRad(0.0f, 0.0f, NOD_SETTLE_MS, ease_sin, true); }, NOD_SETTLE_MS);
}

HeadNod::~HeadNod() {
}

// fraction: 0 = level, 1 = fully down
void HeadNod::tilt(float fraction, uint ms) {
	pHead->movePanRad(0.0f, ms, ease_sin, true);
	pHead->moveTiltRad(pHead->getTiltDownRad() * fraction, ms, ease_sin, true);
}
