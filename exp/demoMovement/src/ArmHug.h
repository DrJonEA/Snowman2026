/*
 * ArmHug.h
 *
 * Brings both arms forward into a hug, then wiggles them.
 */

#ifndef EXP_ARMS_SRC_ARMHUG_H_
#define EXP_ARMS_SRC_ARMHUG_H_

#include "pico/stdlib.h"
#include "Anim.h"
#include "Arm.h"

class ArmHug : public Anim {
public:
	ArmHug(Arm *left, Arm *right, uint wiggles = 4, bool loop = false);
	virtual ~ArmHug();

private:
	void moveBoth(ArmPos pos, uint ms);
	// fraction: 1.0 = fully forward, lower eases back toward rest
	void wiggleBoth(float fraction, uint ms);

	Arm *pLeft;
	Arm *pRight;
};

#endif /* EXP_ARMS_SRC_ARMHUG_H_ */
