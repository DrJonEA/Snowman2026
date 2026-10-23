/*
 * RadarTarget.h
 *
 * The single target the snowman reacts to: the stationary target if the radar
 * has one, otherwise the nearest moving target.
 */

#ifndef RADARTARGET_H_
#define RADARTARGET_H_

#include <stdint.h>

struct RadarTarget {
	bool valid;          // a target is present
	bool moving;         // true = moving target, false = stationary
	int16_t angle_tenth; // degrees x 10
	uint16_t dist_cm;
};
typedef struct RadarTarget RadarTarget_t;

#endif /* RADARTARGET_H_ */
