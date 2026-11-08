# MuLIB logger

[Manuals](README.md) · [Library index](index.md)

Simple callback-driven logging with message severity filtering. Logging does not allocate, format printf arguments, or access global state. Caller owns the sink function and user pointer.

## Contract and behavior

`init()` stores a sink callback, caller context, and minimum severity;
it does not own them. `write()` invokes the sink synchronously only when the
message level meets the threshold. A filtered message still returns 1; NULL
logger/message/sink returns 0. The callback has no return status, so sink I/O
failure is not propagated. Messages are borrowed for the callback duration.
No formatting, copying, queueing, allocation, or internal synchronization occurs.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_LOGGER
#include "mulib-logger.h"
static void mulib_example_sink(void *u, mulib_logger_level_t level,
                               const char *message) {
    (void)u;
    (void)level;
    (void)message;
}
int main(void) {
    mulib_logger_t l;
    mulib_logger_init(&l, mulib_example_sink, NULL, MULIB_LOG_INFO);
    assert(mulib_logger_write(&l, MULIB_LOG_INFO, "test"));
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-logger.h`](../include/mulib-logger.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-logger.h
 * @brief Simple callback-driven logging with message severity filtering.
 * @defgroup mulib_logger MuLIB logger
 * @{
 * @details Logging does not allocate, format printf arguments, or access
 * global state. Caller owns the sink function and user pointer.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_LOGGER before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_LOGGER_H
#define MULIB_LOGGER_H
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Simple callback-driven logging with message severity filtering. */
  typedef enum mulib_logger_level
  {
    MULIB_LOG_DEBUG = 0,
    MULIB_LOG_INFO = 1,
    MULIB_LOG_WARN = 2,
    MULIB_LOG_ERROR = 3
  } mulib_logger_level_t;
  /** @brief Simple callback-driven logging with message severity filtering. */
  typedef void (*mulib_logger_sink) (void *user, mulib_logger_level_t level,
                                     const char *message);
  /** @brief Simple callback-driven logging with message severity filtering. */
  typedef struct mulib_logger
  {
    mulib_logger_sink sink;
    void *user;
    mulib_logger_level_t minimum;
  } mulib_logger_t;
  /** @brief mulib logger init; see manual/mulib-logger.md for contracts and
   * examples. */
  void mulib_logger_init (mulib_logger_t *l, mulib_logger_sink sink,
                          void *user, mulib_logger_level_t minimum);
  /** @brief mulib logger write; see manual/mulib-logger.md for contracts and
   * examples. */
  int mulib_logger_write (mulib_logger_t *l, mulib_logger_level_t level,
                          const char *message);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_LOGGER_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-logger.h`](../include/mulib-logger.h) |
| Implementation switch | `MULIB_IMPL_LOGGER` |
| Repository-local includes | None |
| Complete example | [`examples/modules/logger.c`](../examples/modules/logger.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/logger.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
