# MuLIB microtcl

[Manuals](README.md) · [Library index](index.md)

An embeddable Tcl-inspired command interpreter with a documented small builtin set. This interpreter supports a small, deliberately documented Tcl-like subset: `set`, `unset`, `append`, `incr`, arithmetic `expr a + b`, and `puts`. It recognizes semicolons/newlines, whitespace, quoted/braced words, escapes, and `$name` substitution. It does not implement the full Tcl grammar, procedures, Tcl library loading, namespaces, command substitution, or safe sandboxing. `loadall()` creates the interpreter with all *MuLIB-provided* commands enabled.

## Contract and behavior

`loadall()` creates the interpreter with all MuLIB built-ins: `set`,
`unset`, `append`, `incr`, `expr`, and `puts`. It does not load external Tcl
packages. Input is NUL-terminated; commands are separated by newline/semicolon,
with at most 64 words. Double quotes and braces group words; `$name` substitutes
variables outside braces. There is no nested command substitution or full Tcl
grammar. `expr` accepts two numeric operands with one `+`, `-`, `*`, or `/`.

Eval returns 1 on completion and 0 on syntax, command, arithmetic, or allocation
failure. Earlier variable changes and output are not rolled back. Get/result
return borrowed strings invalidated by replacement or destruction. `destroy()`
frees interpreter state. `puts` writes to stdout; its I/O result is not a distinct
interpreter error.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_MICROTCL
#include "mulib-microtcl.h"
int main(void) {
    mulib_microtcl_t *t = mulib_microtcl_loadall();
    assert(t);
    assert(mulib_microtcl_eval(t, "set x 3; incr x"));
    assert(!strcmp(mulib_microtcl_result(t), "4"));
    mulib_microtcl_destroy(t);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-microtcl.h`](../include/mulib-microtcl.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-microtcl.h
 * @brief An embeddable Tcl-inspired command interpreter with a documented
 * small builtin set.
 * @defgroup mulib_microtcl MuLIB microtcl
 * @{
 * @details This interpreter supports a small, deliberately documented Tcl-like
 * subset: `set`, `unset`, `append`, `incr`, arithmetic `expr a + b`, and
 * `puts`. It recognizes semicolons/newlines, whitespace, quoted/braced words,
 * escapes, and `$name` substitution. It does not implement the full Tcl
 * grammar, procedures, Tcl library loading, namespaces, command substitution,
 * or safe sandboxing. `loadall()` creates the interpreter with all
 * *MuLIB-provided* commands enabled.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_MICROTCL before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_MICROTCL_H
#define MULIB_MICROTCL_H
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

  /** @brief An embeddable Tcl-inspired command interpreter with a documented
   * small builtin set. */
  typedef struct mulib_microtcl_var
  {
    char *name, *value;
    struct mulib_microtcl_var *next;
  } mulib_microtcl_var_t;
  /** @brief An embeddable Tcl-inspired command interpreter with a documented
   * small builtin set. */
  typedef struct mulib_microtcl
  {
    mulib_microtcl_var_t *vars;
    char *result;
    int errors;
  } mulib_microtcl_t;
  /** @brief An embeddable Tcl-inspired command interpreter with a documented
   * small builtin set. */
  typedef mulib_microtcl_t mulib_micrtcl_t;
  /** @brief mulib microtcl loadall; see manual/mulib-microtcl.md for contracts
   * and examples. */
  mulib_microtcl_t *mulib_microtcl_loadall (void);
  /** @brief mulib microtcl eval; see manual/mulib-microtcl.md for contracts
   * and examples. */
  int mulib_microtcl_eval (mulib_microtcl_t *t, const char *script);
  /** @brief mulib microtcl result; see manual/mulib-microtcl.md for contracts
   * and examples. */
  const char *mulib_microtcl_result (const mulib_microtcl_t *t);
  /** @brief mulib microtcl get; see manual/mulib-microtcl.md for contracts and
   * examples. */
  const char *mulib_microtcl_get (const mulib_microtcl_t *t, const char *name);
  /** @brief mulib microtcl destroy; see manual/mulib-microtcl.md for contracts
   * and examples. */
  void mulib_microtcl_destroy (mulib_microtcl_t *t);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_MICROTCL_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-microtcl.h`](../include/mulib-microtcl.h) |
| Implementation switch | `MULIB_IMPL_MICROTCL` |
| Repository-local includes | None |
| Complete example | [`examples/modules/microtcl.c`](../examples/modules/microtcl.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/microtcl.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
