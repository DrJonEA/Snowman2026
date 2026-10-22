/*
 * SnowmanBrain.h
 *
 * Behaviour state machine for the snowman. Radar readings become events and
 * the state machine chooses which animations to play.
 *
 *   Asleep
 *   Awake
 *     WakingUp -> Idle <-> Looking
 *                 Idle/Looking -> Greeting -> Entertaining -> Idle
 *
 * To add a behaviour: add a State in the constructor, give it onEnter/onExit
 * actions, and add on(...) rules for the events it reacts to.
 */

#ifndef FIRMWARE_SRC_SNOWMANBRAIN_H_
#define FIRMWARE_SRC_SNOWMANBRAIN_H_

#include "StateMachine.h"
#include "Anim.h"
#include "Head.h"
#include "HeadNod.h"
#include "HeadShake.h"
#include "Star.h"
#include "StarChase.h"
#include "StarHeartbeat.h"

enum BrainEvent : uint16_t {
	EvHumanAppeared = 1,
	EvHumanGone,
	EvHumanNear,      // value = distance cm
	EvHumanFar,
	EvAnimDone,
	EvSleepNow,
	EvTargetFound,    // stationary target appeared
	EvTargetLost
};

// One radar target as reported by the sensor.
struct RadarTarget {
	bool valid = false;
	int16_t angleTenth = 0;   // degrees x 10
	uint16_t distanceCm = 0;
};

enum TrackMode : uint8_t {
	TrackNone,
	TrackStationary,   // look at the stationary target
	TrackMoving        // no stationary target: follow the nearest moving one
};

// Everything the behaviours can drive.
struct BrainActors {
	Head *head;
	Star *star;
	StarChase *chase;
	StarHeartbeat *heart;
	Anim *wake;
	Anim *look;
	Anim *greet;
	Anim *dance;
	Anim *sleep;
	HeadNod *nod;
	HeadShake *shake;
	Anim *waveLeft;
	Anim *waveRight;
};

class SnowmanBrain {
public:
	SnowmanBrain(const BrainActors &actors);

	void start();
	void poll();

	// Feed the latest radar reading; posts events on changes.
	void updateSensor(bool human, uint16_t distanceCm);

	// Latest radar targets. A stationary target is looked at; if there is none
	// the nearest moving target is followed. Call every loop.
	void updateTargets(const RadarTarget &stationary, const RadarTarget &moving0,
			const RadarTarget &moving1);

	StateMachine &machine() { return xSm; }

private:
	bool humanNear() const { return xNear; }
	bool canEngage();
	bool animRunning() const;
	bool targetInFront() const;
	float targetPanRad() const;   // stationary target
	float trackPanRad() const;    // whatever is being tracked
	void faceTarget(StateMachine &sm);

	BrainActors xA;
	StateMachine xSm;

	State xAsleep;
	State xAwake;
	State xWakingUp;
	State xIdle;
	State xLooking;
	State xGreeting;
	State xEntertaining;
	State xAttending;   // facing a stationary target and gesturing
	State xAttNod;
	State xAttShake;
	State xAttWave;
	State xAttPause;

	bool xHuman = false;
	bool xNear = false;
	uint xIdleCycles = 0;
	uint32_t xCooldownUntilMs = 0;

	// A target held across short radar dropouts, with a low-pass filtered angle.
	struct Held {
		bool valid = false;
		float angleDeg = 0.0f;
		uint16_t distanceCm = 0;
		uint32_t seenMs = 0;
	};
	void updateHeld(Held &h, bool valid, int16_t angleTenth, uint16_t dist,
			uint32_t now, uint32_t dtMs, uint32_t holdMs, uint32_t tauMs);

	Held xStat;
	Held xMov;
	TrackMode xTrackMode = TrackNone;
	uint32_t xLastUpdateMs = 0;
	Anim *pAttWave = nullptr;
	float xFacingRad = 1000.0f;  // 1000 = unknown
	uint32_t xLastTurnMs = 0;
	uint32_t xLastTelemetryMs = 0;
};

#endif /* FIRMWARE_SRC_SNOWMANBRAIN_H_ */
