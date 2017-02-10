#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fsm.h"

#include <klee/klee.h>

static fsm_state_t fsm_state;

void fsm_init() { fsm_state = 0; }

void fsm_transition(fsm_transition_t transition) {
  if (transition) {
    fsm_state++;
  }

  if (fsm_state >= 3) {
    fsm_state = 0;
  }
}

fsm_state_t fsm_getstate() { return fsm_state; }
