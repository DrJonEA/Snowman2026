/*
 * Head.cpp
 */

#include "Head.h"

#define CENTRE_RAD ((float)M_PI / 2.0f)

// Absolute servo limits in radians; adjust to the head's mechanical range.
#define PAN_MIN_RAD   (CENTRE_RAD - 0.785f)
#define PAN_MAX_RAD   (CENTRE_RAD + 0.785f)
#define TILT_MIN_RAD  (CENTRE_RAD - 0.436f)
#define TILT_MAX_RAD  (CENTRE_RAD + 0.436f)

// Tilt servo is mounted inverted: positive tilt (up) lowers the servo angle.
#define TILT_DIR      (-1.0f)

Head::Head(Servo *pan, Servo *tilt) {
	pPan = pan;
	pTilt = tilt;
	xPanMin = PAN_MIN_RAD;
	xPanMax = PAN_MAX_RAD;
	xTiltMin = TILT_MIN_RAD;
	xTiltMax = TILT_MAX_RAD;

	servo_set_rad(pPan, CENTRE_RAD);
	servo_set_rad(pTilt, CENTRE_RAD);
}

Head::~Head() {
}

bool Head::moveServo(Servo *servo, float r, float minRad, float maxRad,
		uint ms, ease_fn_t ease_fn, bool approx) {
	float dir = (servo == pTilt) ? TILT_DIR : 1.0f;
	float target = dir * r + CENTRE_RAD;
	if (target < minRad || target > maxRad) {
		if (!approx) {
			return false;
		}
		target = target < minRad ? minRad : maxRad;
	}
	servo_time_to_rad(servo, target, ms * 1000, ease_fn, NULL);
	return true;
}

bool Head::movePanRad(float pan, uint ms, ease_fn_t ease_fn, bool approx) {
	return moveServo(pPan, pan, xPanMin, xPanMax, ms, ease_fn, approx);
}

bool Head::moveTiltRad(float tilt, uint ms, ease_fn_t ease_fn, bool approx) {
	return moveServo(pTilt, tilt, xTiltMin, xTiltMax, ms, ease_fn, approx);
}

bool Head::moveRad(float pan, float tilt, uint ms, ease_fn_t ease_fn, bool approx) {
	// Check both first so a rejected move doesn't leave one servo moved.
	if (!approx) {
		float p = pan + CENTRE_RAD;
		float t = TILT_DIR * tilt + CENTRE_RAD;
		if (p < xPanMin || p > xPanMax || t < xTiltMin || t > xTiltMax) {
			return false;
		}
	}
	bool a = movePanRad(pan, ms, ease_fn, approx);
	bool b = moveTiltRad(tilt, ms, ease_fn, approx);
	return a && b;
}

bool Head::movePosName(HeadPos name, uint ms, ease_fn_t ease_fn) {
	switch (name) {
	case HeadUp:
		return moveRad(0.0f, getTiltUpRad(), ms, ease_fn, true);
	case HeadDown:
		return moveRad(0.0f, getTiltDownRad(), ms, ease_fn, true);
	case HeadLeft:
		return moveRad(xPanMax - CENTRE_RAD, 0.0f, ms, ease_fn, true);
	case HeadRight:
		return moveRad(xPanMin - CENTRE_RAD, 0.0f, ms, ease_fn, true);
	default:
		return moveRad(0.0f, 0.0f, ms, ease_fn, true);
	}
}

float Head::getPanRad() {
	return (pPan->_current_deg * M_PI / 180) - CENTRE_RAD;
}

float Head::getTiltRad() {
	return TILT_DIR * ((pTilt->_current_deg * M_PI / 180) - CENTRE_RAD);
}

float Head::getTiltUpRad() {
	return xTiltMax - CENTRE_RAD;
}

float Head::getTiltDownRad() {
	return xTiltMin - CENTRE_RAD;
}
