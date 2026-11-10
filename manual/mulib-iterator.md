# iterator — borrowed cursor protocol

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_ITERATOR`.

Configure borrowed state, next callback and optional close callback. Next returns
the callback's status unchanged: conventionally 1 item, 0 end, negative error.
Invalid iterator/output or absent next callback returns -1. The callback sets
the borrowed item and defines its lifetime; the wrapper does not clear outputs
on end/error or latch exhaustion. Close calls the cleanup callback once then
zeros the iterator, making repeated close safe. The callback owns any state
cleanup. No allocations or synchronization are provided by the wrapper.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-iterator.h`](../include/mulib-iterator.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Opaque cursor protocol with explicit error and borrowed-item semantics. C11
 * single-header; define MULIB_IMPL_ITERATOR once. */
#ifndef MULIB_ITERATOR_H
#define MULIB_ITERATOR_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    void *state;
    int (*next) (void *, const void **);
    void (*close) (void *);
  } mulib_iterator_t;
  int mulib_iterator_next (mulib_iterator_t *it, const void **item);
  void mulib_iterator_close (mulib_iterator_t *it);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-iterator.h`](../include/mulib-iterator.h) |
| Implementation switch | `MULIB_IMPL_ITERATOR` |
| Repository-local includes | None |
| Complete example | [`examples/modules/iterator.c`](../examples/modules/iterator.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/iterator.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
