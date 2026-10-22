/*
 * Head.h
 *
 * Two-servo head: pan (left/right) and tilt (up/down).
 * Angles are radians offset from centre; positive pan is left, positive tilt is up.
 */

#ifndef EXP_ARMS_SRC_HEAD_H_
#define EXP_ARMS_SRC_HEAD_H_

#include "pico/stdlib.h"

#include <math.h>
extern "C" {
#include "servo.h"
}
#include "Kinematics.h"

enum HeadPos {
	HeadCentre,
	HeadUp,
	HeadDown,
	HeadLeft,
	HeadRight
};

class Head {
public:
	Head(Servo *pan, Servo *tilt);
	virtual ~Head();

	bool movePosName(HeadPos name, uint ms, ease_fn_t ease_fn = NULL);
	bool moveRad(float pan, float tilt, uint ms, ease_fn_t ease_fn = NULL, bool approx = false);
	bool movePanRad(float pan, uint ms, ease_fn_t ease_fn = NULL, bool approx = false);
	bool moveTiltRad(float tilt, uint ms, ease_fn_t ease_fn = NULL, bool approx = false);
	bool movePos(float x, float y, float z, uint ms, ease_fn_t ease_fn = NULL, bool approx = false);

	// Pan offset added to every pan move, so animations written around 0 rad
	// become relative to this facing direction. Does not move the head.
	void setPanBias(float rad) { xPanBias = rad; }
	float getPanBias() const { return xPanBias; }

	float getPanRad();
	float getTiltRad();
	void getPos(float &x, float &y, float &z);

	// Tilt limits as offsets from centre, for building custom moves.
	float getTiltUpRad();
	float getTiltDownRad();

private:
	bool moveServo(Servo *servo, float r, float minRad, float maxRad,
			uint ms, ease_fn_t ease_fn, bool approx);

	Servo *pPan;
	Servo *pTilt;
	Kin::Chain xChain;
	float xPanBias = 0.0f;
	float xPanMin, xPanMax;
	float xTiltMin, xTiltMax;
};

#endif /* EXP_ARMS_SRC_HEAD_H_ */
