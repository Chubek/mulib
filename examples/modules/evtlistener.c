#define MULIB_IMPL_EVTSTREAM
#define MULIB_IMPL_EVTLISTENER
#include "mulib-evtlistener.h"
#include <assert.h>

static int receive(const mulib_event_t *event, void *user) {
  size_t *count = (size_t *)user;
  assert(event->node && event->depth == 0);
  ++*count; return 1;
}
int main(void) {
  mulib_ast_t node = {0};
  size_t count = 0;
  mulib_evtlistener_t listener = {receive, &count};
  mulib_evtstream_t stream;
  mulib_event_t event = {MULIB_EVENT_ENTER, &node, 0};
  node.production = (char *)"root";
  stream = mulib_evtstream_from_ast(&node);
  assert(mulib_evtlistener_listen(&stream, &listener) == 1 && count == 2);
  /* A parser can also send an event backed by this temporary node. */
  assert(mulib_evtlistener_push(&listener, &event) == 1 && count == 3);
  return 0;
}
