/*
 * Anim.h
 *
 * Generic polled animation: an ordered list of timed steps.
 */

#ifndef EXP_ARMS_SRC_ANIM_H_
#define EXP_ARMS_SRC_ANIM_H_

#include "pico/stdlib.h"
#include <functional>
#include <vector>

class Anim {
public:
	struct Step {
		std::function<void()> action; // run when the step begins
		uint32_t durationMs;          // time before the next step begins
	};

	Anim(bool loop = true);
	virtual ~Anim();

	void addStep(std::function<void()> action, uint32_t durationMs);
	void clear();

	void start();
	void stop();

	// Call frequently; advances steps when their time has elapsed.
	void poll();

	bool isRunning() const;
	void setLoop(bool loop);

private:
	void beginStep(size_t i, absolute_time_t at);

	std::vector<Step> xSteps;
	size_t xIndex = 0;
	bool xRunning = false;
	bool xLoop;
	absolute_time_t xNext;
};

#endif /* EXP_ARMS_SRC_ANIM_H_ */
