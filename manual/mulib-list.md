# MuLIB list

[Manuals](README.md) · [Library index](index.md)

Intrusive doubly linked circular list with sentinel. Nodes belong to caller-defined structs. Initialize sentinel with mulib_list_init, then link previously unlinked nodes. Never insert a node already linked into a list.

## Contract and behavior

The root is a caller-owned sentinel initialized with `init()`. Nodes
are intrusive: embed them in your objects and keep those objects alive while
linked. Append/prepend splice a detached node at the tail/head; they do not copy
or allocate it. Never insert a node already linked elsewhere or the sentinel
itself. `remove()` unlinks and clears the node's links without freeing its owner.
An initialized empty sentinel points to itself. Operations are constant-time;
there is no membership validation, destructor, or internal locking.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_LIST
#include "mulib-list.h"
int main(void) {
    mulib_list_node_t root, node;
    mulib_list_init(&root);
    mulib_list_append(&root, &node);
    assert(!mulib_list_empty(&root));
    mulib_list_remove(&node);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-list.h`](../include/mulib-list.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-list.h
 * @brief Intrusive doubly linked circular list with sentinel.
 * @defgroup mulib_list MuLIB list
 * @{
 * @details Nodes belong to caller-defined structs. Initialize sentinel with
 * mulib_list_init, then link previously unlinked nodes. Never insert a node
 * already linked into a list.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_LIST before including this
 * file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_LIST_H
#define MULIB_LIST_H
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

  /** @brief Intrusive doubly linked circular list with sentinel. */
  typedef struct mulib_list_node
  {
    struct mulib_list_node *prev, *next;
  } mulib_list_node_t;
  /** @brief mulib list init; see manual/mulib-list.md for contracts and
   * examples. */
  void mulib_list_init (mulib_list_node_t *root);
  /** @brief mulib list empty; see manual/mulib-list.md for contracts and
   * examples. */
  int mulib_list_empty (const mulib_list_node_t *root);
  /** @brief mulib list append; see manual/mulib-list.md for contracts and
   * examples. */
  void mulib_list_append (mulib_list_node_t *root, mulib_list_node_t *node);
  /** @brief mulib list prepend; see manual/mulib-list.md for contracts and
   * examples. */
  void mulib_list_prepend (mulib_list_node_t *root, mulib_list_node_t *node);
  /** @brief mulib list remove; see manual/mulib-list.md for contracts and
   * examples. */
  void mulib_list_remove (mulib_list_node_t *node);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_LIST_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-list.h`](../include/mulib-list.h) |
| Implementation switch | `MULIB_IMPL_LIST` |
| Repository-local includes | None |
| Complete example | [`examples/modules/list.c`](../examples/modules/list.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/list.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
