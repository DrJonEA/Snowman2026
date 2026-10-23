/*
 * SnowmanBrain.cpp
 */

#include "SnowmanBrain.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#define NEAR_CM            150   // human is "near" inside this distance
#define FAR_CM             200   // and "far" again beyond this (hysteresis)
#define IDLE_LOOK_MS       8000  // idle time before looking around
#define IDLE_CYCLES_SLEEP  3     // looks with nobody about before sleeping
#define ENGAGE_COOLDOWN_MS 20000 // pause after a dance before greeting again

// Radar angle sign relative to head pan (positive pan is left). Set to -1.0f
// if the head turns away from the target.
#define RADAR_TO_PAN_SIGN   1.0f
#define NEG_ANGLE_GAIN      1.1f   // +10% on negative angles to compensate for weaker response that way
#define TURN_MAX_RAD        0.7f
// Stationary target: slow, smooth turn, never interrupting a turn in progress.
#define LOOK_MOVE_MS        2000
// Moving target: quick follow, as the SkullHead firmware does.
#define FOLLOW_MOVE_MS      50
#define FOLLOW_MIN_DIFF_RAD 0.01f
#define FOLLOW_SETTLE_MS    300    // after this long without a move, fine-tune
#define FOLLOW_SETTLE_RAD   0.001f
#define FRONT_MAX_RAD       0.8f   // "in front" = within about 45 degrees
#define STAT_HOLD_MS        1000   // keep a lost stationary target this long (radar flicker)
#define TELEMETRY_MS        1000   // periodic status print
#define ATT_PAUSE_MS        2500   // gap between gestures when the target is under 1 m
#define ATT_PAUSE_FAR_MS    15000  // gap at 3 m or more

// Chase colour by target distance: >2 m green, 2-1 m green to blue,
// 1-0.7 m blue to red, <0.7 m red.
static PicoLed::Color distanceColour(uint16_t cm) {
	auto mix = [](float f, int r0, int g0, int b0, int r1, int g1, int b1) {
		return PicoLed::RGB((uint8_t)(r0 + (r1 - r0) * f), (uint8_t)(g0 + (g1 - g0) * f),
				(uint8_t)(b0 + (b1 - b0) * f));
	};
	if (cm >= 200) {
		return PicoLed::RGB(0, 255, 0);
	}
	if (cm >= 100) {
		return mix((200 - cm) / 100.0f, 0, 255, 0, 0, 0, 255);
	}
	if (cm > 70) {
		return mix((100 - cm) / 30.0f, 0, 0, 255, 255, 0, 0);
	}
	return PicoLed::RGB(255, 0, 0);
}

static const PicoLed::Color ChaseColour = PicoLed::RGB(116, 215, 238);
static const PicoLed::Color HeartAwake = PicoLed::RGB(228, 3, 3);
static const PicoLed::Color HeartAsleep = PicoLed::RGB(0, 76, 255);

SnowmanBrain::SnowmanBrain(const BrainActors &actors) :
		xA(actors),
		xAsleep("Asleep"),
		xAwake("Awake"),
		xWakingUp("WakingUp", &xAwake),
		xIdle("Idle", &xAwake),
		xLooking("Looking", &xAwake),
		xGreeting("Greeting", &xAwake),
		xEntertaining("Entertaining", &xAwake),
		xAttending("Attending", &xAwake),
		xAttNod("AttNod", &xAttending),
		xAttShake("AttShake", &xAttending),
		xAttWave("AttWave", &xAttending),
		xAttPause("AttPause", &xAttending) {

	// Asleep: sleep pose, slow dim heartbeat, ring off.
	xAsleep.onEnter([this](StateMachine &) {
		xA.chase->stop();
		for (uint i = 0; i < STAR_RING_LEN; i++) {
			xA.star->setRing(i, PicoLed::RGB(0, 0, 0));
		}
		xA.star->show();
		xA.heart->setColour(HeartAsleep);
		xA.heart->setSpeed(4000);
		xA.head->setPanBias(0.0f);
		xFacingRad = 1000.0f;
		xA.sleep->start();
	}).onExit([this](StateMachine &) {
		xA.sleep->stop();
	}).on(EvHumanAppeared, &xAwake)
	  .on(EvTargetFound, &xAwake);

	// Awake: lively lights; any child can be told to go back to sleep.
	xAwake.initial(&xWakingUp).onEnter([this](StateMachine &) {
		xA.chase->setColour(ChaseColour);
		xA.chase->start();
		xA.heart->setColour(HeartAwake);
		xA.heart->setSpeed(1000);
		xIdleCycles = 0;
	}).on(EvSleepNow, &xAsleep)
	  .on(EvHumanAppeared, nullptr, nullptr, [this](StateMachine &) { xIdleCycles = 0; });

	// Each animation state starts its animation, stops it on exit and posts
	// EvAnimDone once it finishes.
	for (auto p : { std::make_pair(&xWakingUp, xA.wake), std::make_pair(&xLooking, xA.look),
			std::make_pair(&xGreeting, xA.greet), std::make_pair(&xEntertaining, xA.dance) }) {
		Anim *a = p.second;
		p.first->onEnter([a](StateMachine &) { a->start(); })
				.onExit([a](StateMachine &) { a->stop(); })
				.onTick([a](StateMachine &sm) {
					if (!a->isRunning()) {
						sm.post(EvAnimDone);
					}
				});
	}

	// Wake and look-around are written around centre, so clear the facing bias.
	for (State *st : { &xWakingUp, &xLooking }) {
		st->onEnter([this](StateMachine &) { xA.head->setPanBias(0.0f); xFacingRad = 1000.0f; });
	}
	xWakingUp.on(EvAnimDone, &xIdle);

	// Idle: engage if someone is near, sleep if nobody has been about, else look around.
	xIdle.timeout(IDLE_LOOK_MS)
		.onTick([this](StateMachine &sm) {
			if (targetInFront()) {
				sm.post(EvTargetFound);
			} else {
				faceTarget(sm);
			}
		})
		// Head position is unknown after other states, so force a re-aim on return.
		.onEnter([this](StateMachine &sm) {
			if (!xAttending.isAncestorOrSelf(sm.previous())) {
				xFacingRad = 1000.0f;
			}
		})
		.on(EvHumanNear, &xGreeting, [this](StateMachine &sm, const Event &) { return canEngage(); })
		.on(EvTimeout, &xAsleep, [this](StateMachine &, const Event &) {
			return !xHuman && !xStat.valid && !xMov.valid && xIdleCycles >= IDLE_CYCLES_SLEEP;
		})
		.on(EvTimeout, &xGreeting, [this](StateMachine &, const Event &) { return xNear && canEngage(); })
		.on(EvTimeout, &xAttending, [this](StateMachine &, const Event &) { return targetInFront(); })
		.on(EvTargetFound, &xAttending, [this](StateMachine &, const Event &) { return targetInFront(); })
		.on(EvTimeout, &xLooking);

	xLooking.onEnter([this](StateMachine &) { xIdleCycles++; })
		.on(EvHumanNear, &xGreeting, [this](StateMachine &, const Event &) { return canEngage(); })
		.on(EvAnimDone, &xIdle);

	// Greeting: dance if the human stayed near, otherwise carry on.
	xGreeting.on(EvAnimDone, &xEntertaining, [this](StateMachine &, const Event &) { return xNear; })
		.on(EvAnimDone, &xIdle);

	// Attending: facing a stationary target in front; nod, shake and wave in turn
	// like joining a conversation. The wave uses the hand opposite the target.
	xAttending.initial(&xAttNod)
		.onEnter([this](StateMachine &sm) {
			if (!xIdle.isAncestorOrSelf(sm.previous())) {
				xFacingRad = 1000.0f;
			}
		})
		.onTick([this](StateMachine &sm) { faceTarget(sm); })
		.on(EvTargetLost, &xIdle)
		.on(EvHumanNear, &xGreeting, [this](StateMachine &, const Event &) { return canEngage(); });

	xAttNod.onEnter([this](StateMachine &) {
		xA.nod->start();
	}).onExit([this](StateMachine &) { xA.nod->stop(); })
	  .onTick([this](StateMachine &sm) { if (!xA.nod->isRunning()) sm.post(EvAnimDone); });

	xAttShake.onEnter([this](StateMachine &) {
		xA.shake->start();
	}).onExit([this](StateMachine &) { xA.shake->stop(); })
	  .onTick([this](StateMachine &sm) { if (!xA.shake->isRunning()) sm.post(EvAnimDone); });

	xAttWave.onEnter([this](StateMachine &) {
		// Target on the left (positive pan) -> wave the right hand, and vice versa.
		pAttWave = targetPanRad() > 0.0f ? xA.waveRight : xA.waveLeft;
		printf("Wave with %s hand (target pan %.1f deg)\n",
				pAttWave == xA.waveRight ? "right" : "left", targetPanRad() * 180.0f / (float)M_PI);
		pAttWave->start();
	}).onExit([this](StateMachine &) {
		if (pAttWave) {
			pAttWave->stop();
		}
	}).onTick([this](StateMachine &sm) {
		if (pAttWave && !pAttWave->isRunning()) {
			sm.post(EvAnimDone);
		}
	});

	xAttNod.on(EvAnimDone, &xAttPause);
	xAttShake.on(EvAnimDone, &xAttPause);
	xAttWave.on(EvAnimDone, &xAttPause);

	// The next gesture is chosen at random when the pause starts; shaking the
	// head becomes much more likely when the target is closer than 1 m.
	// Wait for the head to stop first (e.g. a slow tracking turn): starting a move
	// on a busy servo would block the main loop and freeze the LED animations.
	xAttPause.onEnter([this](StateMachine &sm) {
			pAttNext = pickGesture();
			xPauseStartMs = sm.nowMs();
			// Under 1 m: normal rate. Beyond that the gap stretches linearly to
			// ATT_PAUSE_FAR_MS at 3 m, with +/-25% jitter so it doesn't feel regular.
			float far = 0.0f;
			if (xTargetCm >= 300) {
				far = 1.0f;
			} else if (xTargetCm > 100) {
				far = (xTargetCm - 100) / 200.0f;
			}
			float gap = ATT_PAUSE_MS + far * (ATT_PAUSE_FAR_MS - ATT_PAUSE_MS);
			float jitter = 0.75f + 0.5f * ((float)(rand() % 1000) / 1000.0f);
			xPauseMs = (uint32_t)(gap * (far > 0.0f ? jitter : 1.0f));
			printf("Next gesture in %u ms (dist %ucm)\n", (unsigned)xPauseMs, xTargetCm);
		})
		.onTick([this](StateMachine &sm) {
			if (sm.nowMs() - xPauseStartMs >= xPauseMs && !xA.head->isMoving()) {
				sm.post(EvTimeout);
			}
		});
	xAttPause
		.on(EvTimeout, &xAttShake, [this](StateMachine &, const Event &) { return pAttNext == &xAttShake; })
		.on(EvTimeout, &xAttWave, [this](StateMachine &, const Event &) { return pAttNext == &xAttWave; })
		.on(EvTimeout, &xAttNod);

	xEntertaining.onExit([this](StateMachine &sm) {
		xCooldownUntilMs = sm.nowMs() + ENGAGE_COOLDOWN_MS;
		xIdleCycles = 0;
	}).on(EvAnimDone, &xIdle);
}

static float clampPan(float r) {
	return r > TURN_MAX_RAD ? TURN_MAX_RAD : (r < -TURN_MAX_RAD ? -TURN_MAX_RAD : r);
}

static float degToPan(float deg) {
	float rad = RADAR_TO_PAN_SIGN * deg * (float)M_PI / 180.0f;
	return rad < 0.0f ? rad * NEG_ANGLE_GAIN : rad;
}

State *SnowmanBrain::pickGesture() {
	// Shake weight ramps from 1 at 1 m or more up to 8 at 0.7 m or less.
	float near = 0.0f;
	if (xTargetCm > 0 && xTargetCm < 100) {
		near = xTargetCm <= 70 ? 1.0f : (100 - xTargetCm) / 30.0f;
	}
	float wNod = 4.0f, wShake = 1.0f + 7.0f * near, wWave = 3.0f;
	float r = (float)(rand() % 1000) / 1000.0f * (wNod + wShake + wWave);
	State *pick = r < wNod ? &xAttNod : (r < wNod + wShake ? &xAttShake : &xAttWave);
	printf("Gesture: dist=%ucm weights nod=%.1f shake=%.1f wave=%.1f -> %s\n",
			xTargetCm, wNod, wShake, wWave, pick->name());
	return pick;
}

// Head pan angle that faces the stationary target (0 if none), clamped.
float SnowmanBrain::targetPanRad() const {
	return xStat.valid ? clampPan(degToPan(xStat.angleDeg)) : 0.0f;
}

// Head pan angle for whatever is being tracked: stationary first, else moving.
float SnowmanBrain::trackPanRad() const {
	switch (xTrackMode) {
	case TrackStationary: return clampPan(degToPan(xStat.angleDeg));
	case TrackMoving: return clampPan(degToPan(xMov.angleDeg));
	default: return 0.0f;
	}
}

bool SnowmanBrain::targetInFront() const {
	return xStat.valid && fabsf(degToPan(xStat.angleDeg)) <= FRONT_MAX_RAD;
}

bool SnowmanBrain::animRunning() const {
	return xA.nod->isRunning() || xA.shake->isRunning() || xA.waveLeft->isRunning()
			|| xA.waveRight->isRunning() || xA.wake->isRunning() || xA.look->isRunning() || xA.greet->isRunning()
			|| xA.dance->isRunning() || xA.sleep->isRunning();
}

// Look at the stationary target if there is one, otherwise follow the nearest
// moving target. With neither, the head stays where it is.
void SnowmanBrain::faceTarget(StateMachine &sm) {
	if (xTrackMode == TrackNone || animRunning()) {
		return;
	}
	uint32_t now = sm.nowMs();
	float want = trackPanRad();
	float diff = xFacingRad > 100.0f ? 1000.0f : fabsf(want - xFacingRad);
	uint32_t sinceTurn = now - xLastTurnMs;
	uint32_t ms;

	if (xTrackMode == TrackStationary) {
		if (sinceTurn < LOOK_MOVE_MS || diff < FOLLOW_SETTLE_RAD) {
			return;
		}
		ms = LOOK_MOVE_MS;
	} else {
		bool far = diff > FOLLOW_MIN_DIFF_RAD;
		bool settle = sinceTurn > FOLLOW_SETTLE_MS && diff > FOLLOW_SETTLE_RAD;
		if (!far && !settle) {
			return;
		}
		ms = FOLLOW_MOVE_MS;
	}
	printf("Turn(%s): radar %.1f deg -> pan %.1f deg (was %.1f, head now %.1f) in %s\n",
			xTrackMode == TrackStationary ? "stationary" : "moving",
			xTrackMode == TrackStationary ? xStat.angleDeg : xMov.angleDeg,
			want * 180.0f / (float)M_PI,
			xFacingRad > 100.0f ? 0.0f : xFacingRad * 180.0f / (float)M_PI,
			xA.head->getPanRad() * 180.0f / (float)M_PI, sm.current()->name());
	xFacingRad = want;
	xLastTurnMs = now;
	xA.head->setPanBias(want);
	xA.head->movePanRad(0.0f, ms, xTrackMode == TrackStationary ? ease_sin : ease_in_expo, true);
}

// Raw angle, no filtering. valid=false keeps the last angle for the hold time
// so brief radar dropouts don't end the interaction.
void SnowmanBrain::updateHeld(Held &h, bool valid, int16_t angleTenth, uint32_t now, uint32_t holdMs) {
	if (valid) {
		h.angleDeg = (float)angleTenth / 10.0f;
		h.seenMs = now;
		h.valid = true;
	} else if (h.valid && now - h.seenMs >= holdMs) {
		h.valid = false;
	}
}

void SnowmanBrain::updateTarget(const RadarTarget_t &target) {
	uint32_t now = xSm.nowMs();

	xTargetCm = target.valid ? target.dist_cm : 0;
	if (target.valid && target.dist_cm > 0) {
		xA.chase->setColour(distanceColour(target.dist_cm));
	}

	bool statWas = xStat.valid;
	updateHeld(xStat, target.valid && !target.moving, target.angle_tenth, now, STAT_HOLD_MS);
	if (xStat.valid != statWas) {
		printf("Stationary target %s (angle %.1f deg)\n", xStat.valid ? "FOUND" : "LOST", xStat.angleDeg);
		xSm.post(xStat.valid ? EvTargetFound : EvTargetLost);
	}

	bool movWas = xMov.valid;
	updateHeld(xMov, target.valid && target.moving, target.angle_tenth, now, 0);
	if (xMov.valid != movWas) {
		printf("Moving target %s (angle %.1f deg)\n", xMov.valid ? "FOUND" : "LOST", xMov.angleDeg);
	}

	TrackMode mode = xStat.valid ? TrackStationary : (xMov.valid ? TrackMoving : TrackNone);
	if (mode != xTrackMode) {
		printf("Tracking: %s\n", mode == TrackStationary ? "stationary" : (mode == TrackMoving ? "moving" : "none"));
		xTrackMode = mode;
	}
}

bool SnowmanBrain::canEngage() {
	return (int32_t)(xSm.nowMs() - xCooldownUntilMs) >= 0;
}

void SnowmanBrain::start() {
	xSm.start(&xAsleep);
}

void SnowmanBrain::poll() {
	xSm.poll();

	uint32_t now = xSm.nowMs();
	if (now - xLastTelemetryMs >= TELEMETRY_MS) {
		xLastTelemetryMs = now;
		printf("Tele: state=%s human=%d near=%d track=%d stat=%d(%.1fdeg) mov=%d(%.1fdeg) trackPan=%.1fdeg "
				"facing=%.1fdeg headPan=%.1fdeg front=%d animRunning=%d\n",
				xSm.current() ? xSm.current()->name() : "-", xHuman, xNear, (int)xTrackMode,
				xStat.valid, xStat.angleDeg, xMov.valid, xMov.angleDeg,
				trackPanRad() * 180.0f / (float)M_PI,
				xFacingRad > 100.0f ? 0.0f : xFacingRad * 180.0f / (float)M_PI,
				xA.head->getPanRad() * 180.0f / (float)M_PI, targetInFront(), animRunning());
	}
}

void SnowmanBrain::updateSensor(bool human, uint16_t distanceCm) {
	if (human != xHuman) {
		xHuman = human;
		xSm.post(human ? EvHumanAppeared : EvHumanGone, distanceCm);
	}
	bool near = xNear;
	if (!human) {
		near = false;
	} else if (distanceCm > 0 && distanceCm < NEAR_CM) {
		near = true;
	} else if (distanceCm >= FAR_CM) {
		near = false;
	}
	if (near != xNear) {
		xNear = near;
		xSm.post(near ? EvHumanNear : EvHumanFar, distanceCm);
	}
}
