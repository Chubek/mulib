# MuLIB sexpr

[Manuals](README.md) · [Library index](index.md)

A tree-based S-expression parser with atoms, lists, and deterministic rendering.
Returned trees own their atom strings; release the whole tree with
`mulib_sexpr_destroy`.

## Contract and behavior

`cont()` creates an owned empty list. `parse()` reads one expression
from NUL-terminated input and optionally reports where it stopped; trailing
text is not automatically rejected. Lists, bare atoms, and quoted atoms are
supported, with a recursive parse/render depth check at 128. NULL indicates
syntax, depth, or allocation failure.

Push transfers an independently owned, detached node only on success (1).
The caller must not share nodes between lists; not every cross-list alias can
be detected. A failed push leaves ownership with the caller. Destroy releases
the node and linked descendants/siblings iteratively. Render returns malloc-
owned text, including quoting/escaping as needed; free it separately. Borrowed
child/atom pointers expire when their owning tree is destroyed.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_SEXPR
#include "mulib-sexpr.h"
int main(void) {
    mulib_sexpr_t *s = mulib_sexpr_parse("(a (b c))", NULL);
    assert(s);
    char *t = mulib_sexpr_render(s);
    assert(t);
    free(t);
    mulib_sexpr_destroy(s);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-sexpr.h`](../include/mulib-sexpr.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-sexpr.h
 * @brief A tree-based S-expression parser with atoms, lists, and deterministic
 * rendering.
 * @defgroup mulib_sexpr MuLIB sexpr
 * @{
 * @details The parser accepts proper nested lists, unquoted atoms, and
 * double-quoted atoms with basic escapes. It has a recursion-depth limit of
 * 128. The returned tree owns all atom strings; destroy releases it iteratively. Parse
 * does not enforce end-of-input; consult the optional end pointer.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_SEXPR before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_SEXPR_H
#define MULIB_SEXPR_H
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

  /** @brief A tree-based S-expression parser with atoms, lists, and
   * deterministic rendering. */
  typedef enum mulib_sexpr_kind
  {
    MULIB_SEXPR_ATOM = 1,
    MULIB_SEXPR_LIST = 2
  } mulib_sexpr_kind_t;
  /** @brief A tree-based S-expression parser with atoms, lists, and
   * deterministic rendering. */
  typedef struct mulib_sexpr
  {
    mulib_sexpr_kind_t kind;
    char *atom;
    struct mulib_sexpr *child, *next;
  } mulib_sexpr_t;
  /** @brief mulib sexpr cont; see manual/mulib-sexpr.md for contracts and
   * examples. */
  mulib_sexpr_t *mulib_sexpr_cont (void);
  /** @brief mulib sexpr parse; see manual/mulib-sexpr.md for contracts and
   * examples. */
  mulib_sexpr_t *mulib_sexpr_parse (const char *source, const char **end);
  /** @brief mulib sexpr push; see manual/mulib-sexpr.md for contracts and
   * examples. */
  int mulib_sexpr_push (mulib_sexpr_t *list, mulib_sexpr_t *value);
  /** @brief mulib sexpr render; see manual/mulib-sexpr.md for contracts and
   * examples. */
  char *mulib_sexpr_render (const mulib_sexpr_t *value);
  /** @brief mulib sexpr destroy; see manual/mulib-sexpr.md for contracts and
   * examples. */
  void mulib_sexpr_destroy (mulib_sexpr_t *value);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_SEXPR_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-sexpr.h`](../include/mulib-sexpr.h) |
| Implementation switch | `MULIB_IMPL_SEXPR` |
| Repository-local includes | None |
| Complete example | [`examples/modules/sexpr.c`](../examples/modules/sexpr.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/sexpr.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
