/*
 * SnowmanBrain.cpp
 */

#include "SnowmanBrain.h"
#include <stdio.h>
#include <math.h>

#define NEAR_CM            150   // human is "near" inside this distance
#define FAR_CM             200   // and "far" again beyond this (hysteresis)
#define IDLE_LOOK_MS       8000  // idle time before looking around
#define IDLE_CYCLES_SLEEP  3     // looks with nobody about before sleeping
#define ENGAGE_COOLDOWN_MS 20000 // pause after a dance before greeting again

// Radar angle sign relative to head pan (positive pan is left). Set to -1.0f
// if the head turns away from the target.
#define RADAR_TO_PAN_SIGN   1.0f
#define TURN_DEADBAND_RAD   0.09f  // ignore changes smaller than ~5 degrees
#define TURN_MIN_INTERVAL_MS 400
#define TURN_MOVE_MS        500
#define TURN_MAX_RAD        0.7f
#define FRONT_MAX_RAD       0.8f   // "in front" = within about 45 degrees
#define STAT_HOLD_MS        2000   // keep a lost stationary target this long (radar flicker)
#define STAT_TAU_MS         600    // angle smoothing for a stationary target
#define MOV_HOLD_MS         1000   // keep a lost moving target this long
#define MOV_TAU_MS          250    // lighter smoothing so moving targets are followed closely
#define FOLLOW_DEADBAND_RAD 0.05f  // following uses finer, quicker moves than looking
#define FOLLOW_MIN_INTERVAL_MS 150
#define FOLLOW_MOVE_MS      250
#define TELEMETRY_MS        1000   // periodic status print
#define ATT_PAUSE_MS        2500   // quiet gap between gestures

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

	// After each pause pick the next gesture in the sequence, using the previous state.
	xAttPause.timeout(ATT_PAUSE_MS)
		.on(EvTimeout, &xAttShake, [this](StateMachine &sm, const Event &) { return sm.previous() == &xAttNod; })
		.on(EvTimeout, &xAttWave, [this](StateMachine &sm, const Event &) { return sm.previous() == &xAttShake; })
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
	return RADAR_TO_PAN_SIGN * deg * (float)M_PI / 180.0f;
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

// Turn the head toward the tracked target, or back to centre if there is none.
void SnowmanBrain::faceTarget(StateMachine &sm) {
	bool follow = xTrackMode == TrackMoving;
	uint32_t interval = follow ? FOLLOW_MIN_INTERVAL_MS : TURN_MIN_INTERVAL_MS;
	if (animRunning() || sm.nowMs() - xLastTurnMs < interval) {
		return;
	}
	float want = trackPanRad();
	float deadband = follow ? FOLLOW_DEADBAND_RAD : TURN_DEADBAND_RAD;
	if (want == xFacingRad || (want != 0.0f && fabsf(want - xFacingRad) < deadband)) {
		return;
	}
	printf("Turn(%s): target %.1f deg -> pan %.1f deg (was %.1f, head now %.1f) in %s\n",
			xTrackMode == TrackStationary ? "stationary" : (follow ? "moving" : "centre"),
			want * 180.0f / (float)M_PI / RADAR_TO_PAN_SIGN, want * 180.0f / (float)M_PI,
			xFacingRad > 100.0f ? 0.0f : xFacingRad * 180.0f / (float)M_PI,
			xA.head->getPanRad() * 180.0f / (float)M_PI, sm.current()->name());
	xFacingRad = want;
	xLastTurnMs = sm.nowMs();
	xA.head->setPanBias(want);
	xA.head->moveRad(0.0f, 0.0f, follow ? FOLLOW_MOVE_MS : TURN_MOVE_MS, ease_sin, true);
}

void SnowmanBrain::updateHeld(Held &h, bool valid, int16_t angleTenth, uint16_t dist,
		uint32_t now, uint32_t dtMs, uint32_t holdMs, uint32_t tauMs) {
	if (valid) {
		float raw = (float)angleTenth / 10.0f;
		if (h.valid) {
			float alpha = 1.0f - expf(-(float)dtMs / (float)tauMs);
			h.angleDeg += alpha * (raw - h.angleDeg);
		} else {
			h.angleDeg = raw;
		}
		h.distanceCm = dist;
		h.seenMs = now;
		h.valid = true;
	} else if (h.valid && now - h.seenMs >= holdMs) {
		h.valid = false;
	}
}

void SnowmanBrain::updateTargets(const RadarTarget &stationary, const RadarTarget &moving0,
		const RadarTarget &moving1) {
	uint32_t now = xSm.nowMs();
	uint32_t dt = xLastUpdateMs ? now - xLastUpdateMs : 0;
	xLastUpdateMs = now;

	bool statWas = xStat.valid;
	updateHeld(xStat, stationary.valid, stationary.angleTenth, stationary.distanceCm,
			now, dt, STAT_HOLD_MS, STAT_TAU_MS);
	if (xStat.valid != statWas) {
		printf("Stationary target %s (angle %.1f deg)\n", xStat.valid ? "FOUND" : "LOST", xStat.angleDeg);
		xSm.post(xStat.valid ? EvTargetFound : EvTargetLost);
	}

	// Follow the nearest valid moving target.
	const RadarTarget *m = nullptr;
	for (const RadarTarget *t : { &moving0, &moving1 }) {
		if (t->valid && (!m || (t->distanceCm > 0 && t->distanceCm < m->distanceCm))) {
			m = t;
		}
	}
	bool movWas = xMov.valid;
	updateHeld(xMov, m != nullptr, m ? m->angleTenth : 0, m ? m->distanceCm : 0,
			now, dt, MOV_HOLD_MS, MOV_TAU_MS);
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
