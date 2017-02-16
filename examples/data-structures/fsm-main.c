#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fsm.h"

#include <klee/klee.h>

void memory_model_loop();

int castan_state_seen(void *state, int size);

int main(int argc, char *argv[]) {
  fsm_init();

  for (unsigned int i = 0; ; i++) {
    fsm_transition_t transition = 0;
#ifdef __clang__
    klee_make_symbolic((void*)&transition, sizeof(transition), "transition");
#endif

    fsm_state_t old_state = fsm_getstate();
#ifdef __clang__
    if (i == 0) {
      assert(! castan_state_seen(&old_state, sizeof(old_state)));
    }
#endif

    fsm_transition(transition);
    fsm_state_t new_state = fsm_getstate();
    printf("State Transition: %d -> %d\n", old_state, new_state);

#ifdef __clang__
    if (castan_state_seen(&new_state, sizeof(new_state))) {
#else
    if (i >= 10) {
#endif
      break;
    }
  }

  return 0;
}
