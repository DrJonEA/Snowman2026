/*
 * StarChase.h
 *
 * Three evenly spaced LEDs chase around the outer ring of the star.
 */

#ifndef EXP_DEMOMOVEMENT_SRC_STARCHASE_H_
#define EXP_DEMOMOVEMENT_SRC_STARCHASE_H_

#include "Anim.h"
#include "Star.h"

class StarChase : public Anim {
public:
	// stepMs: time to fade from one position to the next; smaller is faster
	StarChase(Star *star, PicoLed::Color colour, uint32_t stepMs = 200, bool loop = true);
	virtual ~StarChase();

	void setColour(PicoLed::Color colour);
	void setSpeed(uint32_t stepMs);

private:
	void build();
	void draw(uint pos, uint8_t blend);

	Star *pStar;
	PicoLed::Color xColour;
	uint32_t xStepMs;
};

#endif /* EXP_DEMOMOVEMENT_SRC_STARCHASE_H_ */
