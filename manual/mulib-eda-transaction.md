# mulib-eda-transaction

**Status:** Implemented with the *bounded profile specified below* (not a full EDA/IEEE-standard implementation).  
**Header:** `include/mulib-eda-transaction.h`  
**Implementation switch:** `MULIB_IMPL_EDA_TRANSACTION`  
**Dependencies:** `eda-design-db`.

## Purpose and supported profile

Optimistic owned-snapshot transaction over eda-design-db. Every commit advances the target epoch, including commits with no edits, so concurrent stale snapshots conflict. Commit publishes the staged store under caller serialization. No thread locks or nested transactions.

## Behavior and correctness contract

Zero-initialize a transaction before its first use. `begin` requires an inactive
transaction with no owned staged array, count or capacity. NULL arguments, an
active transaction, leftover staged ownership, and `target == &t->staged` are
rejected. It delegates snapshot allocation/copying to `mulib_eda_db_clone`, then
publishes transaction state only after success. A return of 0 preserves **both
the entire transaction and target** on validation, size-limit or allocation
failure. A return of 1 owns an independent snapshot and borrows the target.
An empty target needs no snapshot allocation.

Edit `t.staged` using the design-DB APIs: existing-record updates, insertions and
deletions remain invisible to the target until commit. Each staged operation has
its own result; a failed staged edit does not implicitly abort the transaction.
Callers must check each result and either retry or roll back. Never shallow-copy
the transaction or its arrays into another owner, and never forge its metadata.

`commit` returns 1 when the target epoch still equals `expected_epoch` and is
not UINT64_MAX. It transfers staged-array ownership into the target, frees the
old target array, and advances the target epoch **once**, irrespective of the
number of staged edits. An unedited or empty commit also advances the epoch,
invalidating other snapshots taken at that epoch. Commit allocates nothing.
The transaction becomes inactive with empty staged storage and a NULL target.

Commit returns 0 without mutation for inactive, conflicting or epoch-exhausted
transactions. On conflict, the transaction remains active with its original
snapshot available for inspection. There is no automatic merge/rebase: roll
back, begin from the latest target, and explicitly reapply the intended edits.
`rollback` releases staged storage and detaches the target without dereferencing
or changing it. NULL and repeated rollback on initialized transactions are safe.
Commit and rollback permit transaction reuse; inactive `expected_epoch` is stale
bookkeeping, not an active revision token.

The target must remain alive at the same address throughout the transaction.
Do not free/reinitialize it, replace it with another snapshot, or directly alter
its epoch: epoch equality cannot distinguish a reset database lifetime (ABA).
Legitimate external `put`, `delete` or other transaction commits are detected by
their epoch changes. All readers/writers must be externally serialized; these
are logical atomic edits, not lock-free/thread-safe publications. Old target
array pointers are invalid after successful commit; staged pointers are invalid
after rollback and must be treated as transferred on successful commit.

Public fields must describe valid live arrays of the declared size. This is not
a validator for caller-forged records, pointers or overlapping ownership. Nested
transactions, persistent journals, schema validation, multi-database transactions
and atomic coordination with an ID pool are outside the supported profile.

## Limits and numerical assumptions

Begin uses O(record count) time and storage with one allocation for a nonempty
snapshot. Commit performs O(1) metadata work plus deallocation; rollback performs
deallocation. Staged DB lookups/edits retain their existing linear scans and
allocation limits. Epochs are uint64_t and never wrap. A snapshot may be read at
epoch UINT64_MAX, but it cannot commit; staged mutations also respect the DB's
epoch limit. Booleans do not distinguish OOM, invalid input, conflict or epoch
exhaustion; a richer status API is deferred. Scalar values retain the DB's finite
double contract, without introducing any new numerical calculations.

## Integration

Compile exactly one C11 source file with `MULIB_IMPL_EDA_TRANSACTION` defined before including the header. Include it without that switch in all other translation units. If dependencies exist, define their implementation switches explicitly in that same source (before all includes), or compile them in separate implementation sources. Link with `-lm` where required.

```c
#define MULIB_IMPL_EDA_DESIGN_DB
#define MULIB_IMPL_EDA_TRANSACTION
#include "mulib-eda-transaction.h"
```

## Focused regression test

`tests/test_eda_transaction.c` is a standalone C11 executable with a substantive positive and negative/boundary test. For example:

The focused suite covers independent snapshots, grouped update/delete/insert,
rollback and reuse, two-snapshot and external-write conflicts, unedited/empty
commits, epoch exhaustion and invalid ownership. `tests/test_eda_alloc.c` injects
begin-allocation and staged-growth failure, checks unchanged state and cleanup,
then retries and commits while further allocations are disabled. The C++ linkage
fixture exercises rollback and commit against a C11 implementation object. The
standalone example demonstrates grouped scalar edits and retained conflict data.

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude tests/test_eda_transaction.c -lm -o /tmp/test_eda_transaction
/tmp/test_eda_transaction
```

## Provenance

Original implementation prepared for Amb3r/MuLIB. No external code was copied. The full requested roadmap is larger than this implemented subset; this module claims only its documented profile.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-eda-transaction.h`](../include/mulib-eda-transaction.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-eda-transaction.h
 * @brief Optimistic owned-snapshot transaction over eda-design-db.
 * @details C11 single-header implementation; define MULIB_IMPL_EDA_TRANSACTION in exactly one
 * implementation translation unit. No mandatory third-party libraries.
 */
#ifndef MULIB_EDA_TRANSACTION_H
#define MULIB_EDA_TRANSACTION_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-eda-design-db.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_eda_transaction {
    mulib_eda_design_db_t *target, staged;
    uint64_t expected_epoch;
    int active;
} mulib_eda_transaction_t;
/** Begin an independent owned snapshot of target; return 1 on success.
 * t must be zero-initialized or inactive with empty staged storage. Rejects
 * NULL, active transactions, nonempty staged storage and target == &t->staged.
 * On invalid input, size overflow or allocation failure, both objects remain
 * unchanged. Uses eda-design-db cloning: O(target->count) time and storage.
 * The target is borrowed and must remain live, at the same address, until
 * commit/rollback. Do not free/reinitialize it or alter its epoch meanwhile.
 * Mutate staged through eda-design-db APIs; do not shallow-copy owned arrays.
 * All access requires caller serialization; no nested transactions or locks.
 */
int mulib_eda_transaction_begin(mulib_eda_transaction_t *t, mulib_eda_design_db_t *target);
/** Publish staged storage if the target epoch still matches; return 1.
 * Allocation-free; advances target epoch once, even for an unedited snapshot,
 * and invalidates pointers into the old target array. Returns 0 on inactive,
 * stale or epoch-exhausted transactions without changing target or staged
 * state. Active transactions remain active on failure for inspection/rollback.
 * Successful
 * commit transfers staged ownership to target and makes t reusable.
 */
int mulib_eda_transaction_commit(mulib_eda_transaction_t *t);
/** Free staged storage, detach the borrowed target and mark t inactive.
 * NULL and repeated rollback are harmless for initialized transactions.
 * Does not access or mutate the target; invalidates staged-array pointers.
 */
void mulib_eda_transaction_rollback(mulib_eda_transaction_t *t);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_EDA_TRANSACTION_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-eda-transaction.h`](../include/mulib-eda-transaction.h) |
| Implementation switch | `MULIB_IMPL_EDA_TRANSACTION` |
| Repository-local includes | [mulib-eda-design-db](mulib-eda-design-db.md) |
| Complete example | [`examples/modules/eda-transaction.c`](../examples/modules/eda-transaction.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/eda-transaction.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
