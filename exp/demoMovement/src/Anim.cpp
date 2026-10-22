/*
 * Anim.cpp
 */

#include "Anim.h"

Anim::Anim(bool loop) : xLoop(loop) {
	xNext = nil_time;
}

Anim::~Anim() {
}

void Anim::addStep(std::function<void()> action, uint32_t durationMs) {
	xSteps.push_back({action, durationMs});
}

void Anim::clear() {
	xRunning = false;
	xSteps.clear();
	xIndex = 0;
}

void Anim::start() {
	if (xSteps.empty()) {
		return;
	}
	xRunning = true;
	beginStep(0, get_absolute_time());
}

void Anim::stop() {
	xRunning = false;
}

bool Anim::isRunning() const {
	return xRunning;
}

void Anim::setLoop(bool loop) {
	xLoop = loop;
}

void Anim::beginStep(size_t i, absolute_time_t at) {
	xIndex = i;
	if (xSteps[i].action) {
		xSteps[i].action();
	}
	// Schedule from the planned time, not now, to avoid drift.
	xNext = delayed_by_ms(at, xSteps[i].durationMs);
}

void Anim::poll() {
	if (!xRunning) {
		return;
	}
	while (xRunning && absolute_time_diff_us(get_absolute_time(), xNext) <= 0) {
		size_t n = xIndex + 1;
		if (n >= xSteps.size()) {
			if (!xLoop) {
				xRunning = false;
				return;
			}
			n = 0;
		}
		beginStep(n, xNext);
	}
}
