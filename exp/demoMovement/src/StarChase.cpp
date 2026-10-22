/*
 * StarChase.cpp
 */

#include "StarChase.h"

#define CHASE_FRAME_MS 20

StarChase::StarChase(Star *star, PicoLed::Color colour, uint32_t stepMs, bool loop) :
		Anim(loop), pStar(star), xColour(colour), xStepMs(stepMs) {
	build();
}

StarChase::~StarChase() {
}

void StarChase::build() {
	clear();
	uint frames = xStepMs / CHASE_FRAME_MS;
	if (frames < 1) {
		frames = 1;
	}
	uint32_t frameMs = xStepMs / frames;
	for (uint i = 0; i < STAR_RING_LEN; i++) {
		for (uint f = 0; f < frames; f++) {
			uint8_t blend = (uint8_t)((f * 255) / frames);
			addStep([this, i, blend]() { draw(i, blend); }, frameMs);
		}
	}
}

// blend 0..255: progress of the fade from this position to the next
void StarChase::draw(uint pos, uint8_t blend) {
	// Only the ring is touched so it can run alongside StarHeartbeat.
	for (uint i = 0; i < STAR_RING_LEN; i++) {
		bool lit = ((i + STAR_RING_LEN - pos) % 2) == 0;
		pStar->setRing(i, Star::scale(xColour, lit ? 255 - blend : blend));
	}
	pStar->show();
}

void StarChase::setColour(PicoLed::Color colour) {
	xColour = colour;
}

void StarChase::setSpeed(uint32_t stepMs) {
	bool wasRunning = isRunning();
	xStepMs = stepMs < 1 ? 1 : stepMs;
	build();
	if (wasRunning) {
		start();
	}
}
