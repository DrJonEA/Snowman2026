/*
 * Dance.cpp
 */

#include "Dance.h"

#define DANCE_PAN 0.25f

Dance::Dance(Head *head, Arm *left, Arm *right, uint32_t beatMs, bool loop)
		: Anim(loop), pHead(head), pLeft(left), pRight(right), xBeatMs(beatMs) {
	// Preparation: head lifts, arms rise gently to a soft position.
	addBeat(0.4f, 0.4f, 0.0f, 0.2f, 2);
	// Port de bras: arms lift together, head follows with a slow sweep.
	addBeat(0.8f, 0.8f,  DANCE_PAN, 0.4f, 2);
	addBeat(0.3f, 0.3f, -DANCE_PAN, 0.3f, 2);
	// Graceful asymmetry: one arm extends while the other lowers, like an arabesque.
	addBeat(0.9f, 0.2f,  DANCE_PAN, 0.5f, 2);
	addBeat(0.2f, 0.9f, -DANCE_PAN, 0.5f, 2);
	// Plie: a soft dip of the head with arms drawn in, then rise.
	addBeat(0.3f, 0.3f, 0.0f, -0.3f, 2);
	addBeat(0.7f, 0.7f, 0.0f,  0.5f, 2);
	// Final bow: slow curtsy, then return to neutral.
	addBeat(0.5f, 0.5f, 0.0f, -0.7f, 3);
	addBeat(0.0f, 0.0f, 0.0f,  0.0f, 3);
}

Dance::~Dance() {
}

void Dance::addBeat(float armL, float armR, float pan, float tilt, float beats) {
	uint ms = (uint)(xBeatMs * beats);
	addStep([this, armL, armR, pan, tilt, ms]() { pose(armL, armR, pan, tilt, ms); }, ms);
}

void Dance::pose(float armL, float armR, float pan, float tilt, uint ms) {
	pLeft->moveRad(pLeft->getForwardRad() * armL, ms, ease_sin, true);
	pRight->moveRad(pRight->getForwardRad() * armR, ms, ease_sin, true);
	float t = tilt >= 0.0f ? pHead->getTiltUpRad() * tilt
			: pHead->getTiltDownRad() * -tilt;
	pHead->moveRad(pan, t, ms, ease_sin, true);
}
