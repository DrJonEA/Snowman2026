/*
 * StarHeartbeat.h
 *
 * The centre LED fades in and out like a heart beat.
 */

#ifndef EXP_DEMOMOVEMENT_SRC_STARHEARTBEAT_H_
#define EXP_DEMOMOVEMENT_SRC_STARHEARTBEAT_H_

#include "Anim.h"
#include "Star.h"

class StarHeartbeat : public Anim {
public:
	// periodMs: duration of one beat; smaller is faster
	StarHeartbeat(Star *star, PicoLed::Color colour, uint32_t periodMs = 1000, bool loop = true);
	virtual ~StarHeartbeat();

	void setColour(PicoLed::Color colour);
	void setSpeed(uint32_t periodMs);

private:
	void build();
	void draw(uint8_t level);

	Star *pStar;
	PicoLed::Color xColour;
	uint32_t xPeriodMs;
};

#endif /* EXP_DEMOMOVEMENT_SRC_STARHEARTBEAT_H_ */
