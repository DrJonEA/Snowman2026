/*
 * StarHeartbeat.cpp
 */

#include "StarHeartbeat.h"
#include <math.h>

#define HB_FRAME_MS 20

StarHeartbeat::StarHeartbeat(Star *star, PicoLed::Color colour, uint32_t periodMs, bool loop) :
		Anim(loop), pStar(star), xColour(colour), xPeriodMs(periodMs) {
	build();
}

StarHeartbeat::~StarHeartbeat() {
}

// Continuous smooth fade in and out, one cycle per period.
static float beatLevel(float t) {
	return 0.5f * (1.0f - cosf(2.0f * (float)M_PI * t));
}

void StarHeartbeat::build() {
	clear();
	uint frames = xPeriodMs / HB_FRAME_MS;
	if (frames < 1) {
		frames = 1;
	}
	uint32_t frameMs = xPeriodMs / frames;
	for (uint f = 0; f < frames; f++) {
		float level = beatLevel((float)f / (float)frames);
		uint8_t v = (uint8_t)(level * 255.0f);
		addStep([this, v]() { draw(v); }, frameMs);
	}
}

void StarHeartbeat::draw(uint8_t level) {
	// Only the centre is touched so it can run alongside StarChase.
	pStar->setCentre(Star::scale(xColour, level));
	pStar->show();
}

void StarHeartbeat::setColour(PicoLed::Color colour) {
	xColour = colour;
}

void StarHeartbeat::setSpeed(uint32_t periodMs) {
	bool wasRunning = isRunning();
	xPeriodMs = periodMs < 1 ? 1 : periodMs;
	build();
	if (wasRunning) {
		start();
	}
}
