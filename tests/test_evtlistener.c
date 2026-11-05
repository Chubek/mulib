#define MULIB_IMPL_EVTLISTENER
#define MULIB_IMPL_EVTSTREAM
#include "mulib-evtlistener.h"
#include <assert.h>
#include <string.h>

typedef struct trace {
  char text[16];
  size_t count, stop;
  int result;
} trace;
static int receive(const mulib_event_t *event, void *user) {
  trace *t = (trace *)user;
  char name = event->node->production[0];
  assert(t->count + 1 < sizeof t->text);
  assert(event->depth == (size_t)(name != 'R'));
  t->text[t->count++] = event->kind == MULIB_EVENT_ENTER ? name : (char)(name + 'a' - 'A');
  t->text[t->count] = 0;
  return t->count == t->stop ? t->result : 7;
}
static int stateless(const mulib_event_t *event, void *user) {
  assert(event && !user); return 2;
}
static void test_stream(void) {
  mulib_ast_t root = {0}, a = {0}, b = {0};
  trace t = {{0}, 0, 0, 0};
  mulib_evtlistener_t listener = {receive, &t}, invalid = {NULL, NULL};
  mulib_evtstream_t stream, before;
  root.production = (char *)"R"; a.production = (char *)"A"; b.production = (char *)"B";
  root.child = &a; root.tail = &b; a.parent = b.parent = &root; a.next = &b;
  stream = mulib_evtstream_from_ast(&root); before = stream;
  assert(mulib_evtlistener_listen(&stream, &invalid) == -1);
  assert(stream.node == before.node && stream.leaving == before.leaving);
  assert(mulib_evtlistener_listen(&stream, &listener) == 1);
  assert(t.count == 6 && !strcmp(t.text, "RAaBbr"));
  assert(mulib_evtlistener_listen(&stream, &listener) == 1 && t.count == 6);
  stream = mulib_evtstream_from_ast(NULL);
  assert(mulib_evtlistener_listen(&stream, &listener) == 1 && t.count == 6);
  assert(mulib_evtlistener_listen(&stream, &invalid) == -1);
  assert(mulib_evtlistener_listen(NULL, &listener) == -1);
  assert(mulib_evtlistener_listen(&stream, NULL) == -1);
  /* Cancellation/error consumes the delivered event and can be resumed. */
  { int result;
    for (result = 0; result >= -1; --result) {
      t.count = 0; t.text[0] = 0; t.stop = 2; t.result = result ? -7 : 0;
      stream = mulib_evtstream_from_ast(&root);
      assert(mulib_evtlistener_listen(&stream, &listener) == result);
      assert(t.count == 2 && !strcmp(t.text, "RA"));
      t.stop = 0;
      assert(mulib_evtlistener_listen(&stream, &listener) == 1);
      assert(t.count == 6 && !strcmp(t.text, "RAaBbr"));
    }
  }
  /* An attached subtree remains bounded and starts at depth zero. */
  { mulib_evtlistener_t simple = {stateless, NULL};
    stream = mulib_evtstream_from_ast(&a);
    assert(mulib_evtlistener_listen(&stream, &simple) == 1 && !stream.node);
  }
}
static void test_push(void) {
  mulib_ast_t node = {0};
  mulib_event_t event = {MULIB_EVENT_ENTER, &node, 0};
  trace t = {{0}, 0, 0, 0};
  mulib_evtlistener_t listener = {receive, &t}, invalid = {NULL, NULL};
  const char payload[] = {'a', 0, 'b'};
  node.production = (char *)"R"; node.text = (char *)payload;
  node.length = sizeof payload; node.offset = 42;
  assert(mulib_evtlistener_push(&listener, &event) == 1);
  event.kind = MULIB_EVENT_LEAVE;
  t.stop = 2; t.result = 0;
  assert(mulib_evtlistener_push(&listener, &event) == 0);
  t.stop = 3; t.result = -23;
  assert(mulib_evtlistener_push(&listener, &event) == -1);
  assert(t.count == 3 && !strcmp(t.text, "Rrr"));
  assert(node.length == 3 && !memcmp(node.text, payload, 3) && node.offset == 42);
  assert(mulib_evtlistener_push(NULL, &event) == -1);
  assert(mulib_evtlistener_push(&invalid, &event) == -1);
  assert(mulib_evtlistener_push(&listener, NULL) == -1);
  event.kind = (mulib_event_kind_t)99;
  assert(mulib_evtlistener_push(&listener, &event) == -1);
  event.kind = MULIB_EVENT_ENTER; event.node = NULL;
  assert(mulib_evtlistener_push(&listener, &event) == -1 && t.count == 3);
  event.node = &node;
  listener.event = stateless; listener.user = NULL;
  assert(mulib_evtlistener_push(&listener, &event) == 1);
}
int main(void) { test_stream(); test_push(); return 0; }
