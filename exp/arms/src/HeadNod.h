/*
 * HeadNod.h
 *
 * Nods the head up and down.
 */

#ifndef EXP_ARMS_SRC_HEADNOD_H_
#define EXP_ARMS_SRC_HEADNOD_H_

#include "pico/stdlib.h"
#include "Anim.h"
#include "Head.h"

class HeadNod : public Anim {
public:
	HeadNod(Head *head, uint nods = 3, bool loop = false);
	virtual ~HeadNod();

private:
	void tilt(float fraction, uint ms);

	Head *pHead;
};

#endif /* EXP_ARMS_SRC_HEADNOD_H_ */
