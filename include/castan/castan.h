#include <klee/klee.h>
#include <string.h>

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
    typeof(input) *input_expr = (typeof(input) *)malloc(sizeof(input));        \
    klee_make_symbolic((void *)input_expr, sizeof(input), "castan_havoc_in");  \
    memcpy((void *)input_expr, (void *)&input, sizeof(input));                 \
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
