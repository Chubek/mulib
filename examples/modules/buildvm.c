#define MULIB_IMPL_ARTIFACT
#define MULIB_IMPL_BUILDGRAPH
#define MULIB_IMPL_VCDIFF
#define MULIB_IMPL_BUILDVM
#include "mulib-buildvm.h"
#include <assert.h>
int main (void)
{
  mulib_buildvm_instruction_t code[2] = {{0}};
  mulib_buildvm_program_t program = {code, 2, 1, 1024};
  mulib_buildgraph_t *g = mulib_buildgraph_create (2, 1);
  mulib_buildgraph_run_t *r = NULL;
  size_t task, output;
  code[0].opcode = MULIB_BUILDVM_LITERAL; code[0].data = "hello"; code[0].length = 5;
  code[1].opcode = MULIB_BUILDVM_EMIT;
  assert (g && !mulib_buildvm_task (g, "generate", &program, &task));
  assert (!mulib_buildgraph_artifact (g, "hello.txt", NULL, &output));
  assert (!mulib_buildgraph_depend (g, task, output));
  assert (!mulib_buildgraph_build (g, &output, 1, NULL, NULL, &r));
  assert (mulib_artifact_view (mulib_buildgraph_value (r, output))->length == 5);
  mulib_buildgraph_destroy (g); mulib_buildgraph_run_destroy (r); return 0;
}
