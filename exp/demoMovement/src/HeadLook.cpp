/*
 * HeadLook.cpp
 */

#include "HeadLook.h"

HeadLook::HeadLook(Head *head, bool loop) : Anim(loop), pHead(head) {
	// Each look is a move step followed by a hold step so the head pauses.
	look( 0.45f, 0.7f, 700, 900);   // up and to the left
	look( 0.20f, 1.0f, 500, 700);   // up high
	look(-0.35f, 0.5f, 800, 1100);  // up to the right
	look(-0.55f, 0.0f, 500, 800);   // far right, level
	look( 0.25f, 0.2f, 900, 600);   // sweep back across, slightly up
	addStep([this]() { pHead->moveRad(0.0f, 0.0f, 800, ease_sin, true); }, 1000);
}

HeadLook::~HeadLook() {
}

void HeadLook::look(float pan, float tiltFraction, uint moveMs, uint holdMs) {
	addStep([this, pan, tiltFraction, moveMs]() {
		pHead->moveRad(pan, pHead->getTiltUpRad() * tiltFraction, moveMs, ease_sin, true);
	}, moveMs);
	addStep(nullptr, holdMs);
}
