/*
 * ArmWave.h
 *
 *  Created on: 19 Oct 2026
 *      Author: jondurrant
 */

#ifndef EXP_ARMS_SRC_ARMWAVE_H_
#define EXP_ARMS_SRC_ARMWAVE_H_

#include "pico/stdlib.h"
#include "Anim.h"
#include "Arm.h"

class ArmWave : public Anim {
public:
	// waves: number of up/down cycles per run
	ArmWave(Arm *a, uint waves = 3, bool loop = false);
	virtual ~ArmWave();

private:
	void move(ArmPos pos, uint ms);

	Arm *pArm;
};

#endif /* EXP_ARMS_SRC_ARMWAVE_H_ */
