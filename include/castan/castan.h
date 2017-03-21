#include <klee/klee.h>

#ifdef __clang__

void castan_loop();
void castan_notify_havoc(int64_t havoc);
#define castan_havoc(var, expr)                                                \
  do {                                                                         \
    typeof(var) havoc;                                                         \
    klee_make_symbolic(&havoc, sizeof(havoc), "castan_havoc");                 \
    if (klee_is_symbolic(havoc)) {                                             \
      var = havoc;                                                             \
    } else {                                                                   \
      var = expr;                                                              \
      castan_notify_havoc(var);                                                \
    }                                                                          \
  } while (0)

#else

#define castan_loop
#define castan_havoc(var, expr)                                                \
  do {                                                                         \
    var = expr;                                                                \
  } while (0)

#endif
