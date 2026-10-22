/*
 * HeadSleep.h
 *
 * Dozes off: the head droops, jerks back awake a couple of times, then
 * settles into a sleep pose (head down, slightly turned, arms at rest).
 */

#ifndef EXP_DEMOMOVEMENT_SRC_HEADSLEEP_H_
#define EXP_DEMOMOVEMENT_SRC_HEADSLEEP_H_

#include "pico/stdlib.h"
#include "Anim.h"
#include "Head.h"
#include "Arm.h"

class HeadSleep : public Anim {
public:
	HeadSleep(Head *head, Arm *left, Arm *right, bool loop = false);
	virtual ~HeadSleep();

private:
	// tiltFraction: 0 = level, 1 = fully down
	void droop(float pan, float tiltFraction, uint ms);
	void arms(ArmPos pos, uint ms);

	Head *pHead;
	Arm *pLeft;
	Arm *pRight;
};

#endif /* EXP_DEMOMOVEMENT_SRC_HEADSLEEP_H_ */
