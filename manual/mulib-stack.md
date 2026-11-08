# MuLIB stack

[Manuals](README.md) · [Library index](index.md)

A last-in-first-out stack of fixed-stride elements. The default element size is sizeof(void *), so callers can immediately store pointers using mulib_stack_push(stack, &ptr). Choose another stride before the first push.

## Contract and behavior

Create returns an owned empty stack or NULL. Its default stride is
`sizeof(void *)`; `set_stride()` requires an empty stack and positive size,
releasing any retained element storage. Set/push/pop return 1 on success, 0 on
failure. Push copies bytes, including from this stack's live storage across
growth; it never assumes ownership of pointed-to objects. Failed push preserves
existing elements.

Pop removes the newest element and allows NULL output to discard it. Peek
returns a borrowed last-element pointer or NULL when empty; growth, removal,
stride changes, and destruction can invalidate the view. Push is amortized
constant-time for fixed stride; pop/peek are constant-time. Destroy frees the
stack and its storage, and accepts NULL.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_STACK
#include "mulib-stack.h"
int main(void) {
    mulib_stack_t *s = mulib_stack_create();
    assert(s);
    int x = 7, y = 0;
    assert(mulib_stack_set_stride(s, sizeof(int)));
    assert(mulib_stack_push(s, &x));
    assert(mulib_stack_pop(s, &y) && y == 7);
    mulib_stack_destroy(s);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-stack.h`](../include/mulib-stack.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-stack.h
 * @brief A last-in-first-out stack of fixed-stride elements.
 * @defgroup mulib_stack MuLIB stack
 * @{
 * @details The default element size is sizeof(void *), so callers can
 * immediately store pointers using mulib_stack_push(stack, &ptr). Choose
 * another stride before the first push.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_STACK before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_STACK_H
#define MULIB_STACK_H
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

  /** @brief A last-in-first-out stack of fixed-stride elements. */
  typedef struct mulib_stack
  {
    unsigned char *data;
    size_t length, capacity, stride;
  } mulib_stack_t;
  /** @brief mulib stack create; see manual/mulib-stack.md for contracts and
   * examples. */
  mulib_stack_t *mulib_stack_create (void);
  /** @brief mulib stack set stride; see manual/mulib-stack.md for contracts
   * and examples. */
  int mulib_stack_set_stride (mulib_stack_t *s, size_t stride);
  /** @brief Copy an element; a borrowed element/peek from this stack is safe
   * across growth. */
  int mulib_stack_push (mulib_stack_t *s, const void *value);
  /** @brief mulib stack pop; see manual/mulib-stack.md for contracts and
   * examples. */
  int mulib_stack_pop (mulib_stack_t *s, void *out);
  /** @brief mulib stack peek; see manual/mulib-stack.md for contracts and
   * examples. */
  const void *mulib_stack_peek (const mulib_stack_t *s);
  /** @brief mulib stack destroy; see manual/mulib-stack.md for contracts and
   * examples. */
  void mulib_stack_destroy (mulib_stack_t *s);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_STACK_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-stack.h`](../include/mulib-stack.h) |
| Implementation switch | `MULIB_IMPL_STACK` |
| Repository-local includes | None |
| Complete example | [`examples/modules/stack.c`](../examples/modules/stack.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/stack.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
