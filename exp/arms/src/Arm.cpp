/*
 * Arm.cpp
 *
 *  Created on: 16 Oct 2026
 *      Author: jondurrant
 */

#include "Arm.h"

// Measured dimensions in metres; the rod bend is 30 deg in radians.
#define ARM_MOUNT_X       0.0f
#define ARM_MOUNT_Y       0.0f
#define ARM_MOUNT_Z       0.09f
#define ARM_BRACKET_Y     0.0236f
#define ARM_BRACKET_Z     0.0f
#define ARM_ROD_LEN1      0.0228f
#define ARM_ROD_LEN2      0.0455f
#define ARM_ROD_BEND_RAD  0.5236f

Arm::Arm(Servo* servo, bool left) {
	pServo = servo;
	xLeft = left;

	if (left){
		xMinRad = 0.436332; // deg 25.0;
		xMaxRad = 2.0944  ;// deg 120.0;
	} else {
		xMinRad = 1.0472; // deg 60.0;
		xMaxRad = 2.61799; //deg 150.0;
	}

	// Body frame: X forward, Y left, Z up. Joint angle equals moveRad's r;
	// the axis flips per side so positive r swings the tip backward on both.
	float side = left ? 1.0f : -1.0f;
	xChain.add(std::make_unique<Kin::FixedJoint>(
			Kin::Vec3(ARM_MOUNT_X, side * ARM_MOUNT_Y, ARM_MOUNT_Z)));
	xChain.add(std::make_unique<Kin::FixedJoint>(
			Kin::Vec3(0.0f, side * ARM_BRACKET_Y, ARM_BRACKET_Z)));
	xChain.add(std::make_unique<Kin::RevoluteJoint>(Kin::Vec3(0.0f, 0.0f, side)));
	// Rods point outward at rest and bend about Z toward the front.
	xChain.add(std::make_unique<Kin::Rod>(
			ARM_ROD_LEN1, ARM_ROD_LEN2, Kin::Vec3(0.0f, side, 0.0f),
			Kin::Vec3::UnitZ(), -side * ARM_ROD_BEND_RAD));

	servo_set_rad(pServo, M_PI / 2.0);
}

Arm::~Arm() {
	// TODO Auto-generated destructor stub
}

bool Arm::moveRad(float r,  uint ms, ease_fn_t ease_fn, bool approx){
	float target = r + M_PI / 2.0;
	if (target < xMinRad){
		if (approx)
			target = xMinRad;
		else
			return false;
	}
	if (target > xMaxRad){
		if (approx)
			target = xMaxRad;
		else
			return false;
	}

	servo_time_to_rad(
			pServo,
			target,
			ms * 1000,
			ease_fn,
			NULL);
	return true;
}

bool Arm::movePosName(ArmPos name, uint ms, ease_fn_t ease_fn){
	// Limits are absolute servo radians; moveRad takes an offset from centre.
	const float centre = M_PI / 2.0;
	switch(name){
	case ArmForward:
		return moveRad((isLeft() ? xMinRad : xMaxRad) - centre, ms, ease_fn, true);
	case ArmBack:
		return moveRad((isLeft() ? xMaxRad : xMinRad) - centre, ms, ease_fn, true);
	default:
		return moveRad(0.0, ms, ease_fn, true);
	}
}

bool Arm::movePos(float x, float y, float z,  uint ms, ease_fn_t ease_fn, bool approx){
	float q;
	bool reached = xChain.ik(Kin::Vec3(x, y, z), q);
	if (!reached && !approx){
		return false;
	}
	return moveRad(q, ms, ease_fn, approx);
}


ArmPos Arm::getPosName(bool approx){
	return ArmUnknown;
}

float Arm::getRad(){
	float r = (pServo->_current_deg * M_PI/180) - (M_PI / 2.0);
	return r;
}

void Arm::getPos(float &x, float &y, float &z){
	Kin::Vec3 p = xChain.position(getRad());
	x = p.x();
	y = p.y();
	z = p.z();
}

bool Arm::isLeft(){
	return xLeft;
}

float Arm::getForwardRad(){
	return (xLeft ? xMinRad : xMaxRad) - M_PI / 2.0;
}
