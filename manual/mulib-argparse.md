# MuLIB argparse

[Manuals](README.md) · [Library index](index.md)

Lightweight GNU-style options and positional arguments parser. Parses `--key=value`, `--key value`, `-k value`, bare flags defaulting to "1", and operands after `--`. Short clusters are not expanded. Value interpretation is deliberately heuristic; a negative-number argument should be given with `=` to avoid option ambiguity. Positionals borrow argv pointers.

## Contract and behavior

`load()` allocates an empty parser; `destroy()` frees it and accepts
NULL. `parse()` skips `argv[0]`, accepts `--key=value`, `--key value`, and single-
dash names, and stops option recognition at `--`. A flag without a following
non-dash argument gets the string value `"1"`; short-option clusters are not split.
Repeated keys replace their earlier values. Parsing again accumulates entries;
failure can leave earlier entries installed.

Option names and values are copied. `get()` returns a borrowed value or NULL;
replacement and destruction invalidate that value. Positional strings borrow
the original `argv` storage. `parse()` returns 1 on success and 0 on invalid
arguments or allocation failure; `has()` is a membership predicate.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_ARGPARSE
#include "mulib-argparse.h"
int main(void) {
    char *argv[] = {"app", "--mode=fast", "file.txt"};
    mulib_argparse_t *a = mulib_argparse_load();
    assert(a);
    assert(mulib_argparse_parse(a, 3, argv));
    assert(!strcmp(mulib_argparse_get(a, "mode"), "fast"));
    mulib_argparse_destroy(a);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-argparse.h`](../include/mulib-argparse.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-argparse.h
 * @brief Lightweight GNU-style options and positional arguments parser.
 * @defgroup mulib_argparse MuLIB argparse
 * @{
 * @details Parses `--key=value`, `--key value`, `-k value`, bare flags
 * defaulting to "1", and operands after `--`. Short clusters are not expanded.
 * Value interpretation is deliberately heuristic; a negative-number argument
 * should be given with `=` to avoid option ambiguity. Positionals borrow argv
 * pointers.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_ARGPARSE before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_ARGPARSE_H
#define MULIB_ARGPARSE_H
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

  /** @brief Lightweight GNU-style options and positional arguments parser. */
  typedef struct mulib_argparse_option
  {
    char *key, *value;
  } mulib_argparse_option_t;
  /** @brief Lightweight GNU-style options and positional arguments parser. */
  typedef struct mulib_argparse
  {
    mulib_argparse_option_t *items;
    size_t length, capacity;
    char **positionals;
    size_t positional_count;
  } mulib_argparse_t;
  /** @brief mulib argparse load; see manual/mulib-argparse.md for contracts
   * and examples. */
  mulib_argparse_t *mulib_argparse_load (void);
  /** @brief mulib argparse parse; see manual/mulib-argparse.md for contracts
   * and examples. */
  int mulib_argparse_parse (mulib_argparse_t *a, int argc, char *const argv[]);
  /** @brief mulib argparse get; see manual/mulib-argparse.md for contracts and
   * examples. */
  const char *mulib_argparse_get (const mulib_argparse_t *a, const char *name);
  /** @brief mulib argparse has; see manual/mulib-argparse.md for contracts and
   * examples. */
  int mulib_argparse_has (const mulib_argparse_t *a, const char *name);
  /** @brief mulib argparse destroy; see manual/mulib-argparse.md for contracts
   * and examples. */
  void mulib_argparse_destroy (mulib_argparse_t *a);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_ARGPARSE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-argparse.h`](../include/mulib-argparse.h) |
| Implementation switch | `MULIB_IMPL_ARGPARSE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/argparse.c`](../examples/modules/argparse.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/argparse.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
