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

	float getPanRad();
	float getTiltRad();

	// Tilt limits as offsets from centre, for building custom moves.
	float getTiltUpRad();
	float getTiltDownRad();

private:
	bool moveServo(Servo *servo, float r, float minRad, float maxRad,
			uint ms, ease_fn_t ease_fn, bool approx);

	Servo *pPan;
	Servo *pTilt;
	float xPanMin, xPanMax;
	float xTiltMin, xTiltMax;
};

#endif /* EXP_ARMS_SRC_HEAD_H_ */
