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
#if defined(MULIB_IMPL_DBTRANSACTION)                                         \
    && !defined(MULIB_DBTRANSACTION_IMPLEMENTED)
#define MULIB_DBTRANSACTION_IMPLEMENTED
int
mulib_dbtransaction_begin (mulib_dbtransaction_t *tx, mulib_memtable_t *t)
{
  int rc;
  if (!tx || !t || tx->target || &tx->working == t)
    return -1;
  rc = mulib_memtable_clone (t, &tx->working);
  if (!rc)
    {
      tx->target = t;
      tx->base_revision = t->revision;
    }
  return rc;
}
int
mulib_dbtransaction_commit (mulib_dbtransaction_t *tx)
{
  if (!tx || !tx->target)
    return -1;
  if (tx->target->revision != tx->base_revision)
    return 1;
  if (tx->base_revision == UINT64_MAX)
    return -3;
  mulib_memtable_free (tx->target);
  *tx->target = tx->working;
  tx->target->revision = tx->base_revision + 1;
  tx->working.records = NULL;
  tx->working.count = tx->working.capacity = 0;
  tx->working.revision = 0;
  tx->target = NULL;
  tx->base_revision = 0;
  return 0;
}
void
mulib_dbtransaction_abort (mulib_dbtransaction_t *tx)
{
  if (tx)
    {
      mulib_memtable_free (&tx->working);
      tx->target = NULL;
      tx->base_revision = 0;
    }
}
#endif
