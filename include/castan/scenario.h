#include <klee/klee.h>

#ifdef __clang__
__attribute__((weak)) enum {
  NONE,
  BEST_CASE,
  WORST_CASE,
} scenario = NONE;

void __attribute__((weak)) scenario_init() {
  if (scenario == NONE) {
    int choice;
    klee_make_symbolic(&choice, sizeof(choice), "scenario");
    if (choice) {
      scenario = BEST_CASE;
    } else {
      scenario = WORST_CASE;
    }
  }
}
#else
void __attribute__((weak)) scenario_init() {}
#endif
