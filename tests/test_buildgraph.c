#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static long budget = -1;
static void *allocate (size_t n) {
  if (budget == 0) return NULL;
  if (budget > 0) --budget;
  return malloc (n);
}
static void *allocate_zero (size_t n, size_t size) {
  void *p;
  if (size && n > SIZE_MAX/size) return NULL;
  p = allocate (n*size);
  if (p) memset (p, 0, n*size);
  return p;
}
#define malloc allocate
#define calloc allocate_zero
#define MULIB_IMPL_ARTIFACT
#define MULIB_IMPL_BUILDGRAPH
#include "mulib-buildgraph.h"
#undef malloc
#undef calloc
struct Action { int fail, omit; };
static int destroyed, calls;
static void destroy_object (void *p) { assert (p == &destroyed); ++destroyed; }
static mulib_artifact_t *bytes (void) {
  mulib_artifact_desc_t d = {0};
  const char value[] = { 'a', 0, 'b' };
  d.id = "message:1"; d.type = "message"; d.data = value; d.length = sizeof (value);
  return mulib_artifact_create (&d);
}
static void artifacts (void) {
  mulib_artifact_desc_t d = {0};
  mulib_artifact_t *a;
  long i;
  assert (!mulib_artifact_create (NULL));
  assert (!mulib_artifact_create (&d));
  d.id = "agent:result"; d.type = "agent.object"; d.kind = MULIB_ARTIFACT_OBJECT;
  d.object = &destroyed; d.destroy = destroy_object;
  for (i = 0; i < 3; ++i) {
    budget = i; assert (!mulib_artifact_create (&d)); assert (!destroyed);
  }
  budget = -1; a = mulib_artifact_create (&d);
  assert (a && mulib_artifact_retain (a) == a);
  mulib_artifact_release (a); assert (!destroyed);
  mulib_artifact_release (a); assert (destroyed == 1);
  d.kind = MULIB_ARTIFACT_REFERENCE; d.object = NULL; d.destroy = NULL;
  assert (!mulib_artifact_create (&d));
  d.locator = "agent://session/42"; a = mulib_artifact_create (&d); assert (a);
  assert (!strcmp (mulib_artifact_view (a)->locator, d.locator)); mulib_artifact_release (a);
  a = bytes (); assert (a && mulib_artifact_view (a)->length == 3);
  assert (!memcmp (mulib_artifact_view (a)->data, "a\0b", 3)); mulib_artifact_release (a);
  d.kind = MULIB_ARTIFACT_BYTES; d.data = NULL; d.length = 1;
  assert (!mulib_artifact_create (&d));
  d.length = 0; a = mulib_artifact_create (&d); assert (a); mulib_artifact_release (a);
  assert (!mulib_artifact_view (NULL) && !mulib_artifact_retain (NULL));
  mulib_artifact_release (NULL);
}
static int execute (void *user, const struct Action *a, mulib_buildgraph_step_t *step) {
  size_t i, count, outputs;
  const mulib_buildgraph_binding_t *in = mulib_buildgraph_inputs (step, &count);
  const mulib_buildgraph_binding_t *out = mulib_buildgraph_outputs (step, &outputs);
  mulib_buildgraph_t *graph = (mulib_buildgraph_t *)user;
  assert (count == 1 && in[0].artifact && outputs);
  assert (mulib_buildgraph_depend (graph, in[0].node, out[0].node) == MULIB_BUILDGRAPH_INVALID);
  assert (mulib_buildgraph_artifact (graph, "mutation", NULL, &i) == MULIB_BUILDGRAPH_BUSY);
  ++calls;
  if (!a->omit) for (i = 0; i < outputs; ++i) {
    assert (!mulib_buildgraph_output (step, i, (mulib_artifact_t *)in[0].artifact));
    assert (!mulib_buildgraph_output (step, i, (mulib_artifact_t *)in[0].artifact));
  }
  return a->fail;
}
typedef struct translation { size_t nodes, edges; unsigned seen; int stop; } translation;
static int visit_node (void *user, size_t id, const mulib_buildgraph_node_t *node) {
  translation *t = (translation *)user;
  assert (node && node->name); t->seen |= 1u << id; ++t->nodes;
  return t->stop;
}
static int visit_edge (void *user, size_t before, size_t after) {
  translation *t = (translation *)user;
  assert ((t->seen & (1u << before)) && (t->seen & (1u << after))); ++t->edges; return 0;
}
static void graphs (void) {
  struct Action action = {0, 0}, second = {0, 0};
  mulib_buildgraph_t *g = mulib_buildgraph_create (8, 8);
  mulib_artifact_t *a = bytes ();
  mulib_buildgraph_run_t *run = NULL;
  mulib_buildgraph_state_t state;
  mulib_buildgraph_translator_t tr = { visit_node, visit_edge };
  translation t = {0, 0, 0, 0};
  size_t input, act, output, side, act2, output2, unused, other;
  int code;
  long i;
  assert (g && a);
  assert (mulib_buildgraph_artifact (g, "../escape", a, &input) == MULIB_BUILDGRAPH_INVALID);
  assert (!mulib_buildgraph_artifact (g, "input", a, &input));
  assert (!mulib_buildgraph_action (g, "action", &action, &act));
  assert (!mulib_buildgraph_artifact (g, "output", NULL, &output));
  assert (!mulib_buildgraph_artifact (g, "side", NULL, &side));
  assert (!mulib_buildgraph_action (g, "action2", &second, &act2));
  assert (!mulib_buildgraph_artifact (g, "output2", NULL, &output2));
  assert (!mulib_buildgraph_artifact (g, "unused", a, &unused));
  assert (!mulib_buildgraph_action (g, "other", &second, &other));
  assert (mulib_buildgraph_artifact (g, "input", a, &input) == MULIB_BUILDGRAPH_INVALID);
  assert (mulib_buildgraph_artifact (g, "extra", a, &input) == MULIB_BUILDGRAPH_FULL);
  assert (mulib_buildgraph_depend (g, act, input) == MULIB_BUILDGRAPH_INVALID);
  assert (!mulib_buildgraph_depend (g, input, act));
  assert (!mulib_buildgraph_depend (g, input, act));
  assert (!mulib_buildgraph_depend (g, act, output));
  assert (!mulib_buildgraph_depend (g, act, side));
  assert (mulib_buildgraph_depend (g, act2, side) == MULIB_BUILDGRAPH_INVALID);
  assert (!mulib_buildgraph_depend (g, output, act2));
  assert (!mulib_buildgraph_depend (g, act2, output2));
  assert (!mulib_buildgraph_validate (g));
  assert (!mulib_buildgraph_translate (g, &output, 1, &tr, &t));
  assert (t.nodes == 4 && t.edges == 3 && !calls);
  t.stop = 73;
  assert (mulib_buildgraph_translate (g, NULL, 0, &tr, &t) == 73);
  for (i = 0; i < 7; ++i) {
    budget = i;
    assert (mulib_buildgraph_execute (g, &output, 1, execute, g, &run) == MULIB_BUILDGRAPH_MEMORY);
    assert (!run && !calls);
  }
  budget = -1;
  assert (!mulib_buildgraph_execute (g, &output2, 1, execute, g, &run));
  assert (calls == 2 && mulib_buildgraph_value (run, side) == a);
  assert (!mulib_buildgraph_result (run, unused, &state, NULL) && state == MULIB_BUILDGRAPH_PENDING);
  mulib_buildgraph_run_destroy (run); run = NULL;
  action.fail = 91;
  assert (mulib_buildgraph_execute (g, &output2, 1, execute, g, &run) == MULIB_BUILDGRAPH_FAILED);
  assert (!mulib_buildgraph_value (run, output) && !mulib_buildgraph_value (run, side));
  assert (!mulib_buildgraph_result (run, act, &state, &code) && state == MULIB_BUILDGRAPH_ERRORED && code == 91);
  assert (!mulib_buildgraph_result (run, output2, &state, &code) && state == MULIB_BUILDGRAPH_BLOCKED);
  mulib_buildgraph_run_destroy (run); run = NULL;
  action.fail = 0; action.omit = 1;
  assert (mulib_buildgraph_execute (g, &output, 1, execute, g, &run) == MULIB_BUILDGRAPH_FAILED);
  assert (!mulib_buildgraph_result (run, act, NULL, &code) && code == MULIB_BUILDGRAPH_MISSING);
  mulib_buildgraph_run_destroy (run); run = NULL;
  action.omit = 0;
  assert (!mulib_buildgraph_execute (g, &output, 1, execute, g, &run));
  mulib_buildgraph_destroy (g); mulib_artifact_release (a);
  assert (mulib_artifact_view (mulib_buildgraph_value (run, output))->length == 3);
  mulib_buildgraph_run_destroy (run);
}
static void invalid_graphs (void) {
  struct Action a = {0, 0};
  mulib_buildgraph_t *g = mulib_buildgraph_create (2, 2);
  mulib_buildgraph_run_t *run = NULL;
  size_t x, y;
  assert (g && !mulib_buildgraph_artifact (g, "generated", NULL, &x));
  assert (mulib_buildgraph_validate (g) == MULIB_BUILDGRAPH_MISSING);
  assert (!mulib_buildgraph_action (g, "cycle", &a, &y));
  assert (!mulib_buildgraph_depend (g, x, y) && !mulib_buildgraph_depend (g, y, x));
  assert (mulib_buildgraph_execute (g, NULL, 0, execute, g, &run) == MULIB_BUILDGRAPH_CYCLE && !run);
  mulib_buildgraph_destroy (g);
  g = mulib_buildgraph_create (0, 0); assert (g && !mulib_buildgraph_validate (g));
  assert (!mulib_buildgraph_execute (g, NULL, 0, execute, g, &run));
  mulib_buildgraph_run_destroy (run); mulib_buildgraph_destroy (g);
  assert (!mulib_buildgraph_create (SIZE_MAX, SIZE_MAX));
  assert (mulib_buildgraph_validate (NULL) == MULIB_BUILDGRAPH_INVALID);
}
int main (void) { artifacts (); graphs (); invalid_graphs (); return 0; }
