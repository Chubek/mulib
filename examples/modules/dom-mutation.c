/* Complete, offline dom-mutation example. */
#define MULIB_IMPL_DOM_MUTATION
#include "mulib-dom-mutation.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_dom_mutation_journal_t journal = { 0 };
  journal.limit = 16;
  assert (mulib_dom_mutation_push (&journal, MULIB_DOM_MUTATION_INSERT, 1, 0));
  assert (journal.records[0].sequence == 1);
  mulib_dom_mutation_clear (&journal);
  return 0;
}
