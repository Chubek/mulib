# dbtransaction

[Manuals](README.md) · [Library index](index.md)

Optimistic snapshot transaction over one shared memtable. Zero initialize both
objects; `begin` clones the target. Read/write `tx.working` using memtable APIs.
Target must outlive the active transaction. `commit` checks its captured revision,
then publishes working data without allocation. `abort` frees unpublished changes.

Results: 0 success, 1 commit conflict (working data retained), negative invalid,
allocation or revision overflow. A successful commit invalidates old target
borrows and makes the transaction inactive. No automatic retry, nested active
begin, multi-table atomicity, locking or disk durability. Direct structure-field
mutation bypasses revisions and is unsupported.
Switches: `MULIB_IMPL_DBTRANSACTION`, `MULIB_IMPL_MEMTABLE`.
Example: `examples/modules/dbtransaction.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-dbtransaction.h`](../include/mulib-dbtransaction.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-dbtransaction.h
 * @brief Optimistic single-table snapshot transactions, no implicit disk
 * commit. Define MULIB_IMPL_DBTRANSACTION and MULIB_IMPL_MEMTABLE once. */
#ifndef MULIB_DBTRANSACTION_H
#define MULIB_DBTRANSACTION_H
#include "mulib-memtable.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Zero init; target must outlive an active transaction. No
   * concurrent threads. */
  typedef struct mulib_dbtransaction
  {
    mulib_memtable_t working;
    mulib_memtable_t *target;
    uint64_t base_revision;
  } mulib_dbtransaction_t;
  /** @brief Clone target; 0 success, negative invalid/OOM. Only inactive
   * transactions. */
  int mulib_dbtransaction_begin (mulib_dbtransaction_t *tx,
                                 mulib_memtable_t *target);
  /** @brief Publish working table only if revision unchanged; 0 success, 1
   * conflict, negative invalid/overflow. Conflict retains working data for
   * inspection/abort. Mutate working with memtable APIs, never target's fields
   * directly. */
  int mulib_dbtransaction_commit (mulib_dbtransaction_t *tx);
  /** @brief Discard working changes and reset. */
  void mulib_dbtransaction_abort (mulib_dbtransaction_t *tx);
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
| Header | [`include/mulib-dbtransaction.h`](../include/mulib-dbtransaction.h) |
| Implementation switch | `MULIB_IMPL_DBTRANSACTION` |
| Repository-local includes | [mulib-memtable](mulib-memtable.md) |
| Complete example | [`examples/modules/dbtransaction.c`](../examples/modules/dbtransaction.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dbtransaction.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
