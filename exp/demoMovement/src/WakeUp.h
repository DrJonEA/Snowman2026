/*
 * WakeUp.h
 *
 * Wakes from the sleep pose: a gentle, slow rise to full
 * attention (head level and centred, arms at rest).
 */

#ifndef EXP_DEMOMOVEMENT_SRC_WAKEUP_H_
#define EXP_DEMOMOVEMENT_SRC_WAKEUP_H_

#include "pico/stdlib.h"
#include "Anim.h"
#include "Head.h"
#include "Arm.h"

class WakeUp : public Anim {
public:
	WakeUp(Head *head, Arm *left, Arm *right, bool loop = false);
	virtual ~WakeUp();

private:
	// tilt: -1 = fully down, 0 = level, 1 = fully up
	void pose(float pan, float tilt, float armL, float armR, uint ms);

	Head *pHead;
	Arm *pLeft;
	Arm *pRight;
};

#endif /* EXP_DEMOMOVEMENT_SRC_WAKEUP_H_ */
