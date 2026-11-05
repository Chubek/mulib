#define MULIB_IMPL_VIZIGEN
#define MULIB_IMPL_ASDL
#define MULIB_IMPL_ARENA
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_BINFILE
#define MULIB_IMPL_BINFILEREADER
#define MULIB_IMPL_BINFILEWRITER
#define MULIB_IMPL_BYCUTILS
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_WALKER
#define MULIB_IMPL_EVTLISTENER
#define MULIB_IMPL_EVTSTREAM
#include "mulib-vizigen.h"
#include <assert.h>

static int number(const mulib_event_t *event, void *user) {
  size_t *count = (size_t *)user;
  assert(event->kind == MULIB_EVENT_LEAVE);
  ++*count; return 1;
}
int main(void) {
  const char source[] = "expr = Num(int value) @leave(number) | Empty";
  mulib_buffer_t bytes = {NULL, 0, 0};
  mulib_vizigen_program_t program;
  mulib_vizigen_runtime_t runtime;
  size_t count = 0;
  mulib_vizigen_action_t action = {"number", number, &count};
  /* Parser adapters may instead push events containing ephemeral nodes. */
  mulib_ast_t node = {0};
  node.production = (char *)"Num"; node.text = (char *)"42"; node.length = 2;
  assert(mulib_vizigen_compile(source, sizeof source - 1, NULL, &bytes, NULL));
  assert(mulib_vizigen_open(&program, bytes.data, bytes.length));
  assert(mulib_vizigen_runtime_init(&runtime, &program, &action, 1));
  assert(mulib_vizigen_walk(&runtime, &node) == 1 && count == 1);
  { mulib_evtlistener_t listener = mulib_vizigen_listener(&runtime);
    mulib_event_t event = {MULIB_EVENT_LEAVE, &node, 0};
    assert(mulib_evtlistener_push(&listener, &event) == 1 && count == 2);
  }
  mulib_buffer_free(&bytes); return 0;
}
