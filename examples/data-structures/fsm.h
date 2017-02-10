typedef unsigned int fsm_state_t;
typedef unsigned int fsm_transition_t;

void fsm_init();
void fsm_transition(fsm_transition_t transition);
fsm_state_t fsm_getstate();
