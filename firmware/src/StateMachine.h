/*
 * StateMachine.h
 *
 * Small polled hierarchical state machine.
 *  - States can be nested; unhandled events bubble up to the parent.
 *  - A transition to a composite state drills down to its initial child
 *    (or its last active child if history is enabled).
 *  - Entry/exit actions run along the path through the common ancestor.
 *  - Events can be posted from callbacks/IRQs; they are handled in poll().
 *  - States can have a timeout which is delivered as EvTimeout.
 */

#ifndef FIRMWARE_SRC_STATEMACHINE_H_
#define FIRMWARE_SRC_STATEMACHINE_H_

#include "pico/stdlib.h"
#include <functional>
#include <vector>

#define EvTimeout 0xFFFF   // Reserved event id, delivered to a state whose timeout expires

struct Event {
	uint16_t id = 0;
	int32_t value = 0;
};

class StateMachine;

class State {
public:
	using Action = std::function<void(StateMachine &)>;
	using Guard = std::function<bool(StateMachine &, const Event &)>;

	State(const char *name, State *parent = nullptr);

	const char *name() const { return pName; }
	State *parent() const { return pParent; }

	// Configuration (chainable)
	// onEnter/onExit append, so they can be called more than once.
	State &onEnter(Action a);
	State &onExit(Action a);
	State &onTick(Action a);
	// Event rule: if the event matches and the guard passes, run the action and
	// transition to target (nullptr = internal, no transition). First match wins.
	State &on(uint16_t id, State *target, Guard guard = nullptr, Action action = nullptr);
	// Deliver EvTimeout after ms in this state (0 = none).
	State &timeout(uint32_t ms);
	// Default child when this composite state is entered.
	State &initial(State *child);
	// Re-enter the last active child instead of the initial one.
	State &history(bool enable = true);

	bool isAncestorOrSelf(const State *s) const;

private:
	friend class StateMachine;

	struct Rule {
		uint16_t id;
		State *target;
		Guard guard;
		Action action;
	};

	bool handle(StateMachine &sm, const Event &ev);
	State *resolve();

	const char *pName;
	State *pParent;
	std::vector<Rule> xRules;
	Action xEnter, xExit, xTick;
	State *pInitial = nullptr;
	State *pLastChild = nullptr;
	bool xHistory = false;
	uint32_t xTimeoutMs = 0;
	uint32_t xEnteredMs = 0;
	bool xTimeoutFired = false;
};

class StateMachine {
public:
	StateMachine();

	// Enter the initial state (drilling down into composites).
	void start(State *initial);

	// Safe to call from callbacks and IRQs.
	bool post(uint16_t id, int32_t value = 0);

	// Call frequently: handles queued events, timeouts, then ticks.
	void poll();

	State *current() const { return pCurrent; }
	State *previous() const { return pPrevious; }
	bool isIn(const State *s) const;
	uint32_t timeInStateMs() const;
	uint32_t nowMs() const;

	void setLogging(bool on) { xLog = on; }

private:
	void dispatch(State *from, const Event &ev);
	void transitionTo(State *target);
	void applyPending();
	bool pop(Event &ev);

	friend class State;

	static const uint QUEUE_LEN = 16;
	Event xQueue[QUEUE_LEN];
	volatile uint xHead = 0;
	volatile uint xTail = 0;

	State *pCurrent = nullptr;
	State *pPrevious = nullptr;
	State *pPending = nullptr;
	bool xLog = true;
};

#endif /* FIRMWARE_SRC_STATEMACHINE_H_ */
