/*
 * Dance.h
 *
 * Dance routine using the head and both arms, in a slow, graceful ballet style.
 */

#ifndef EXP_DEMOMOVEMENT_SRC_DANCE_H_
#define EXP_DEMOMOVEMENT_SRC_DANCE_H_

#include "pico/stdlib.h"
#include "Anim.h"
#include "Head.h"
#include "Arm.h"

class Dance : public Anim {
public:
	// beatMs: length of one beat; smaller is faster
	Dance(Head *head, Arm *left, Arm *right, uint32_t beatMs = 600, bool loop = false);
	virtual ~Dance();

private:
	// armL/armR: 0 = rest, 1 = fully forward. pan rad, tilt 0..1 up / -1..0 down.
	void pose(float armL, float armR, float pan, float tilt, uint ms);
	void addBeat(float armL, float armR, float pan, float tilt, float beats);

	Head *pHead;
	Arm *pLeft;
	Arm *pRight;
	uint32_t xBeatMs;
};

#endif /* EXP_DEMOMOVEMENT_SRC_DANCE_H_ */
