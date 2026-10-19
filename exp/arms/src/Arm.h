/*
 * Arm.h
 *
 *  Created on: 16 Oct 2026
 *      Author: jondurrant
 */

#ifndef EXP_ARMS_SRC_ARM_H_
#define EXP_ARMS_SRC_ARM_H_

#include "pico/stdlib.h"

#include <math.h>
extern "C" {
#include "servo.h"
}
#include "Kinematics.h"

enum ArmPos {
	ArmUnknown,
	ArmForward,
	ArmBack,
	ArmRest
};
class Arm {
public:
	Arm(Servo* servo, bool left = false);
	virtual ~Arm();

	bool movePosName(ArmPos name, uint ms, ease_fn_t ease_fn = NULL);
	bool moveRad(float r,  uint ms, ease_fn_t ease_fn = NULL, bool approx = false);
	bool movePos(float x, float y, float z,  uint ms, ease_fn_t ease_fn = NULL, bool approx = false);


	ArmPos getPosName(bool approx = true);
	float getRad();
	void getPos(float &x, float &y, float &z);

	bool isLeft();

	// Forward limit as an offset from centre, as used by moveRad.
	float getForwardRad();

private:
	Servo *pServo = NULL;
	Kin::Chain xChain;
	bool xLeft;
	float xMinRad, xMaxRad;
};

#endif /* EXP_ARMS_SRC_ARM_H_ */
