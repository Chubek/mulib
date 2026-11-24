/** @file mulib-dom-mutation.h
 * @brief Bounded record journal for DOM mutation events.
 * C11 single-header module. Define MULIB_IMPL_DOM_MUTATION in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_DOM_MUTATION_H
#define MULIB_DOM_MUTATION_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef enum mulib_dom_mutation_kind
  {
    MULIB_DOM_MUTATION_INSERT = 1,
    MULIB_DOM_MUTATION_REMOVE = 2,
    MULIB_DOM_MUTATION_TEXT = 3,
    MULIB_DOM_MUTATION_ATTRIBUTE = 4
  } mulib_dom_mutation_kind_t;
  typedef struct mulib_dom_mutation_record
  {
    mulib_dom_mutation_kind_t kind;
    uint64_t node_id, parent_id, sequence;
  } mulib_dom_mutation_record_t;
  typedef struct mulib_dom_mutation_journal
  {
    mulib_dom_mutation_record_t *records;
    size_t len, cap, limit;
    uint64_t sequence;
  } mulib_dom_mutation_journal_t;
  /** @brief See manual/mulib-dom-mutation.md for this operation's contract. */
  int mulib_dom_mutation_push (mulib_dom_mutation_journal_t *j,
                               mulib_dom_mutation_kind_t kind, uint64_t node,
                               uint64_t parent);
  /** @brief See manual/mulib-dom-mutation.md for this operation's contract. */
  void mulib_dom_mutation_clear (mulib_dom_mutation_journal_t *j);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_DOM_MUTATION_H */
#ifdef MULIB_IMPL_DOM_MUTATION
#ifndef MULIB_IMPL_DOM_MUTATION_ONCE
#define MULIB_IMPL_DOM_MUTATION_ONCE
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
mulib_dom_mutation_push (mulib_dom_mutation_journal_t *j,
                         mulib_dom_mutation_kind_t kind, uint64_t node,
                         uint64_t parent)
{
  if (!j || kind < 1 || kind > 4 || !node || j->sequence == UINT64_MAX
      || (j->limit && j->len >= j->limit))
    return 0;
  if (j->len == j->cap)
    {
      size_t c = j->cap ? 2 * j->cap : 16;
      if (c < j->cap || c > SIZE_MAX / sizeof *j->records)
        return 0;
      void *p = realloc (j->records, c * sizeof *j->records);
      if (!p)
        return 0;
      j->records = p;
      j->cap = c;
    }
  j->records[j->len++]
      = (mulib_dom_mutation_record_t){ kind, node, parent, ++j->sequence };
  return 1;
}
void
mulib_dom_mutation_clear (mulib_dom_mutation_journal_t *j)
{
  if (j)
    {
      free (j->records);
      memset (j, 0, sizeof *j);
    }
}
#endif
#endif
