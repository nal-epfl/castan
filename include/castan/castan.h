#include <klee/klee.h>

#ifdef __clang__

#ifdef __cplusplus
extern "C" {
#endif

void castan_loop();

#ifdef __cplusplus
}
#endif

#define castan_havoc(input, output, expr)                                      \
  do {                                                                         \
    typeof(input) *input_expr = malloc(sizeof(input));                         \
    klee_make_symbolic(input_expr, sizeof(input), "castan_havoc_in");          \
    *input_expr = input;                                                       \
                                                                               \
    typeof(output) havoc;                                                      \
    klee_make_symbolic(&havoc, sizeof(havoc), "castan_havoc_out");             \
    output = havoc;                                                            \
  } while (0)

#else

void castan_loop() {}

#define castan_havoc(input, output, expr)                                      \
  do {                                                                         \
    output = expr;                                                             \
  } while (0)

#endif
