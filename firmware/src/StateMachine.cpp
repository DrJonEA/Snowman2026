/*
 * StateMachine.cpp
 */

#include "StateMachine.h"
#include "hardware/sync.h"
#include <stdio.h>

#define MAX_CHAIN_TRANSITIONS 16

State::State(const char *name, State *parent) : pName(name), pParent(parent) {
}

// Repeated calls chain: earlier actions run first.
static State::Action chain(State::Action first, State::Action next) {
	if (!first) {
		return next;
	}
	return [first, next](StateMachine &sm) { first(sm); next(sm); };
}

State &State::onEnter(Action a) { xEnter = chain(xEnter, a); return *this; }
State &State::onExit(Action a) { xExit = chain(xExit, a); return *this; }
State &State::onTick(Action a) { xTick = a; return *this; }
State &State::timeout(uint32_t ms) { xTimeoutMs = ms; return *this; }
State &State::initial(State *child) { pInitial = child; return *this; }
State &State::history(bool enable) { xHistory = enable; return *this; }

State &State::on(uint16_t id, State *target, Guard guard, Action action) {
	xRules.push_back({id, target, guard, action});
	return *this;
}

bool State::isAncestorOrSelf(const State *s) const {
	for (; s; s = s->pParent) {
		if (s == this) {
			return true;
		}
	}
	return false;
}

bool State::handle(StateMachine &sm, const Event &ev) {
	for (auto &r : xRules) {
		if (r.id != ev.id) {
			continue;
		}
		if (r.guard && !r.guard(sm, ev)) {
			continue;
		}
		if (r.action) {
			r.action(sm);
		}
		if (r.target) {
			sm.transitionTo(r.target);
		}
		return true;
	}
	return false;
}

// Drill down through composite states to the leaf to enter.
State *State::resolve() {
	State *s = this;
	while (true) {
		State *next = (s->xHistory && s->pLastChild) ? s->pLastChild : s->pInitial;
		if (!next) {
			return s;
		}
		s = next;
	}
}

StateMachine::StateMachine() {
}

uint32_t StateMachine::nowMs() const {
	return to_ms_since_boot(get_absolute_time());
}

uint32_t StateMachine::timeInStateMs() const {
	return pCurrent ? nowMs() - pCurrent->xEnteredMs : 0;
}

bool StateMachine::isIn(const State *s) const {
	return s && s->isAncestorOrSelf(pCurrent);
}

bool StateMachine::post(uint16_t id, int32_t value) {
	uint32_t irq = save_and_disable_interrupts();
	uint next = (xHead + 1) % QUEUE_LEN;
	bool ok = next != xTail;
	if (ok) {
		xQueue[xHead] = {id, value};
		xHead = next;
	}
	restore_interrupts(irq);
	return ok;
}

bool StateMachine::pop(Event &ev) {
	uint32_t irq = save_and_disable_interrupts();
	bool ok = xTail != xHead;
	if (ok) {
		ev = xQueue[xTail];
		xTail = (xTail + 1) % QUEUE_LEN;
	}
	restore_interrupts(irq);
	return ok;
}

void StateMachine::start(State *initial) {
	pCurrent = nullptr;
	pPending = initial;
	applyPending();
}

void StateMachine::transitionTo(State *target) {
	pPending = target;
}

void StateMachine::applyPending() {
	for (int n = 0; pPending && n < MAX_CHAIN_TRANSITIONS; n++) {
		State *target = pPending->resolve();
		pPending = nullptr;

		// Deepest proper ancestor of the target that is also on the current path.
		State *lca = nullptr;
		if (pCurrent) {
			for (State *a = target->pParent; a; a = a->pParent) {
				if (a->isAncestorOrSelf(pCurrent)) {
					lca = a;
					break;
				}
			}
		}

		if (xLog) {
			printf("State: %s -> %s\n", pCurrent ? pCurrent->pName : "(none)", target->pName);
		}

		for (State *s = pCurrent; s && s != lca; s = s->pParent) {
			if (s->xExit) {
				s->xExit(*this);
			}
			if (s->pParent) {
				s->pParent->pLastChild = s;
			}
		}

		State *path[16];
		int depth = 0;
		for (State *s = target; s && s != lca && depth < 16; s = s->pParent) {
			path[depth++] = s;
		}

		pPrevious = pCurrent;
		pCurrent = target;
		uint32_t now = nowMs();
		for (int i = depth - 1; i >= 0; i--) {
			path[i]->xEnteredMs = now;
			path[i]->xTimeoutFired = false;
			if (path[i]->xEnter) {
				path[i]->xEnter(*this);
			}
		}
	}
	pPending = nullptr;
}

void StateMachine::dispatch(State *from, const Event &ev) {
	for (State *s = from; s; s = s->pParent) {
		if (s->handle(*this, ev)) {
			return;
		}
	}
}

void StateMachine::poll() {
	if (!pCurrent) {
		return;
	}
	Event ev;
	while (pop(ev)) {
		dispatch(pCurrent, ev);
		applyPending();
	}

	uint32_t now = nowMs();
	for (State *s = pCurrent; s; s = s->pParent) {
		if (s->xTimeoutMs && !s->xTimeoutFired && now - s->xEnteredMs >= s->xTimeoutMs) {
			s->xTimeoutFired = true;
			dispatch(s, {EvTimeout, 0});
			applyPending();
			break;
		}
	}

	// Tick active states, root first. Stop if a tick changed state.
	State *chain[16];
	int n = 0;
	for (State *s = pCurrent; s && n < 16; s = s->pParent) {
		chain[n++] = s;
	}
	for (int i = n - 1; i >= 0; i--) {
		if (chain[i]->xTick) {
			chain[i]->xTick(*this);
			if (pPending) {
				applyPending();
				break;
			}
		}
	}
}
