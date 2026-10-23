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
#include "RadarTarget.h"
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
	void updateTarget(const RadarTarget_t &target);

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

	struct Held {
		bool valid = false;
		float angleDeg = 0.0f;
		uint32_t seenMs = 0;
	};

	void updateHeld(Held &h, bool valid, int16_t angleTenth, uint32_t now, uint32_t holdMs);

	Held xStat;   // stationary target, held briefly across radar dropouts
	Held xMov;    // nearest moving target, latest frame only
	TrackMode xTrackMode = TrackNone;
	Anim *pAttWave = nullptr;
	uint16_t xTargetCm = 0;      // distance to the current target (filtered by the radar callback), 0 = unknown
	uint32_t xLastDistPrintMs = 0;
	uint32_t xPauseStartMs = 0;
	uint32_t xPauseMs = 0;       // gap before the next gesture, set from distance
	State *pAttNext = nullptr;   // gesture chosen for after the pause
	State *pickGesture();
	float xFacingRad = 1000.0f;  // 1000 = unknown
	uint32_t xLastTurnMs = 0;
	uint32_t xLastTelemetryMs = 0;
};

#endif /* FIRMWARE_SRC_SNOWMANBRAIN_H_ */
