#define MULIB_IMPL_WALKER
#define MULIB_IMPL_EVTLISTENER
#define MULIB_IMPL_EVTSTREAM
#include "mulib-walker.h"
#include <assert.h>
static int visit(const mulib_event_t *event, void *user) {
  size_t *count = (size_t *)user;
  assert(event->depth == 0); ++*count; return 1;
}
int main(void) {
  mulib_ast_t node = {0};
  size_t count = 0;
  mulib_evtlistener_t listener = {visit, &count};
  node.production = (char *)"root";
  assert(mulib_walker_walk(&node, &listener) == 1 && count == 2);
  return 0;
}
