#define MULIB_IMPL_ARTIFACT
#define MULIB_IMPL_BUILDGRAPH
#include "mulib-buildgraph.h"
#include <assert.h>
#include <stdio.h>
/* Only the application knows what an Action contains. */
struct Action { const char *description; };
static int execute (void *user, const struct Action *action, mulib_buildgraph_step_t *step) {
  size_t count;
  const mulib_buildgraph_binding_t *input = mulib_buildgraph_inputs (step, &count);
  (void)user; (void)action;
  assert (count == 1);
  /* This example's action forwards an immutable artifact. */
  return mulib_buildgraph_output (step, 0, (mulib_artifact_t *)input[0].artifact);
}
static int translate (void *user, size_t id, const mulib_buildgraph_node_t *node) {
  (void)user;
  printf ("%zu: %s (%s)\n", id, node->name,
          node->kind == MULIB_BUILDGRAPH_ACTION ? "action" : "artifact");
  return 0;
}
int main (void) {
  struct Action copy = { "forward" };
  mulib_artifact_desc_t desc = {0};
  mulib_artifact_t *source;
  mulib_buildgraph_t *graph = mulib_buildgraph_create (3, 2);
  mulib_buildgraph_run_t *run = NULL;
  mulib_buildgraph_translator_t translator = { translate, NULL };
  size_t input, action, output;
  desc.id = "source:hello"; desc.type = "text"; desc.data = "hello"; desc.length = 5;
  source = mulib_artifact_create (&desc);
  assert (graph && source);
  assert (!mulib_buildgraph_artifact (graph, "src/message", source, &input));
  assert (!mulib_buildgraph_action (graph, "actions/copy", &copy, &action));
  assert (!mulib_buildgraph_artifact (graph, "out/message", NULL, &output));
  assert (!mulib_buildgraph_depend (graph, input, action));
  assert (!mulib_buildgraph_depend (graph, action, output));
  assert (!mulib_buildgraph_translate (graph, &output, 1, &translator, NULL));
  assert (!mulib_buildgraph_execute (graph, &output, 1, execute, NULL, &run));
  mulib_buildgraph_destroy (graph); mulib_artifact_release (source);
  assert (mulib_artifact_view (mulib_buildgraph_value (run, output))->length == 5);
  mulib_buildgraph_run_destroy (run);
  return 0;
}
