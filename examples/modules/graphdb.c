#define MULIB_IMPL_GRAPHDB
#define MULIB_IMPL_MEMTABLE
#include "mulib-graphdb.h"
#include <assert.h>
static int
visit (void *context, uint64_t label, uint64_t target)
{
  ++*(int *)context;
  assert (label == 2 && target == 3);
  return 1;
}
int
main (void)
{
  mulib_graphdb_t graph = { 0 };
  int count = 0;
  assert (!mulib_graphdb_edge (&graph, 1, 2, 3, 1, 0));
  assert (mulib_graphdb_neighbors (&graph, 1, visit, &count) == 1
          && count == 1);
  mulib_graphdb_free (&graph);
  return 0;
}
