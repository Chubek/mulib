/* A valid raw C symbol must not collide with a generated function typedef. */
#include <stdint.h>
int64_t on1x_function_type_1(int64_t value) { return value + 1; }
