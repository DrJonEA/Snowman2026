/*
 * HeadShake.h
 *
 * Shakes the head left and right as a "no".
 */

#ifndef EXP_ARMS_SRC_HEADSHAKE_H_
#define EXP_ARMS_SRC_HEADSHAKE_H_

#include "pico/stdlib.h"
#include "Anim.h"
#include "Head.h"

class HeadShake : public Anim {
public:
	HeadShake(Head *head, uint shakes = 3, bool loop = false);
	virtual ~HeadShake();

private:
	void pan(float rad, uint ms);

	Head *pHead;
};

#endif /* EXP_ARMS_SRC_HEADSHAKE_H_ */
