/*
 * HeadLook.h
 *
 * Head looks up and around the room, pausing at a few points before
 * returning to the neutral (0, 0) position.
 */

#ifndef EXP_DEMOMOVEMENT_SRC_HEADLOOK_H_
#define EXP_DEMOMOVEMENT_SRC_HEADLOOK_H_

#include "pico/stdlib.h"
#include "Anim.h"
#include "Head.h"

class HeadLook : public Anim {
public:
	HeadLook(Head *head, bool loop = false);
	virtual ~HeadLook();

private:
	// Glance to a point (pan rad, tilt as fraction of full up), then hold.
	void look(float pan, float tiltFraction, uint moveMs, uint holdMs);

	Head *pHead;
};

#endif /* EXP_DEMOMOVEMENT_SRC_HEADLOOK_H_ */
