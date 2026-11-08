# cowb+tree

[Manuals](README.md) · [Library index](index.md)

Reference-counted snapshots over the existing integer B+ tree. Zero initialize.
`snapshot` shares state into a fresh destination. A write to a shared tree first
rebuilds a private tree; failed allocation leaves both views unchanged. This is
**whole-tree copy on first shared mutation**, not persistent page/path copying.

`set`/`get`/`remove` return 1 success/found and 0 invalid/missing/allocation failure.
Keys are int64 and values uint64. References are single-threaded, not atomic.
Free every snapshot with `free`; do not copy owning handles by assignment.

Switches: `MULIB_IMPL_COWBPTREE`, `MULIB_IMPL_BPTREE`.
Example: `examples/modules/cowb+tree.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-cowb+tree.h`](../include/mulib-cowb+tree.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-cowb+tree.h
 * @brief Reference-counted B+ tree snapshots with whole-tree copy on first
 * write. Define MULIB_IMPL_COWBPTREE and MULIB_IMPL_BPTREE once.
 * Single-threaded refs.
 */
#ifndef MULIB_COWBPTREE_H
#define MULIB_COWBPTREE_H
#include "mulib-b+tree.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_cowbptree_state mulib_cowbptree_state_t;
  typedef struct mulib_cowbptree
  {
    mulib_cowbptree_state_t *state;
  } mulib_cowbptree_t;
  /** @brief Share immutable state into an uninitialized destination; 1
   * succeeds. */
  int mulib_cowbptree_snapshot (const mulib_cowbptree_t *src,
                                mulib_cowbptree_t *out);
  /** @brief Insert/replace, detaching shared storage atomically; zero init
   * required. */
  int mulib_cowbptree_set (mulib_cowbptree_t *t, int64_t key, uint64_t value);
  /** @brief Lookup; 1 found, 0 absent/invalid, optional value unchanged on
   * miss. */
  int mulib_cowbptree_get (const mulib_cowbptree_t *t, int64_t key,
                           uint64_t *value);
  /** @brief Remove; 1 removed, 0 absent or allocation failure. */
  int mulib_cowbptree_remove (mulib_cowbptree_t *t, int64_t key);
  /** @brief Release this reference and reset. */
  void mulib_cowbptree_free (mulib_cowbptree_t *t);
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
| Header | [`include/mulib-cowb+tree.h`](../include/mulib-cowb+tree.h) |
| Implementation switch | `MULIB_IMPL_COWBPTREE` |
| Repository-local includes | [mulib-b+tree](mulib-b+tree.md) |
| Complete example | [`examples/modules/cowb+tree.c`](../examples/modules/cowb+tree.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/cowb+tree.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
