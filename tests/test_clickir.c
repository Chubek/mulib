#define MULIB_IMPL_MIR
#define MULIB_IMPL_GSSAIR
#define MULIB_IMPL_CLICKIR
#include "mulib-clickir.h"
#include <assert.h>
#include <string.h>

static void registers (void)
{
  mulib_mir_native_buffer_t n = {0};
  mulib_clickir_buffer_t c = {0}, copy = {0};
  mulib_clickir_flow_t f = {0};
  mulib_mir_native_program_t p;
  mulib_mir_signature_t sig = {NULL, 0, MULIB_MIR_I64};
  mulib_mir_type_t types[] = {MULIB_MIR_I64, MULIB_MIR_I64, MULIB_MIR_BOOL};
  mulib_mir_frame_t frame = {0};
  size_t k;
  assert (mulib_mir_native_init (&n, 8, 3, 3, 0, 0, 0, 1000) == MULIB_MIR_OK);
  for (k = 0; k < 8; ++k) n.code[k] = mulib_mir_native_instruction (MULIB_MIR_NATIVE_CONST);
  n.blocks[0].first = 0; n.blocks[0].count = 3;
  n.blocks[1].first = 3; n.blocks[1].count = 4;
  n.blocks[2].first = 7; n.blocks[2].count = 1;
  n.code[0].destination = 0; n.code[0].immediate = 5;
  n.code[1].destination = 1; n.code[1].immediate = 1;
  n.code[2] = mulib_mir_native_instruction (MULIB_MIR_NATIVE_JUMP); n.code[2].target = 1;
  n.code[3] = mulib_mir_native_instruction (MULIB_MIR_NATIVE_SUB);
  n.code[3].destination = 0; n.code[3].a = 0; n.code[3].b = 1;
  n.code[4].destination = 1; /* overwrite the decrement with zero */
  n.code[5] = mulib_mir_native_instruction (MULIB_MIR_NATIVE_EQ);
  n.code[5].destination = 2; n.code[5].a = 0; n.code[5].b = 1;
  n.code[6] = mulib_mir_native_instruction (MULIB_MIR_NATIVE_BRANCH);
  n.code[6].a = 2; n.code[6].target = 2; n.code[6].b = 0;
  n.code[7] = mulib_mir_native_instruction (MULIB_MIR_NATIVE_RETURN); n.code[7].a = 0;
  p = mulib_mir_native_view (&n);
  assert (mulib_clickir_from_native (&p, types, sig, 10000, &c, NULL) == MULIB_MIR_OK);
  assert (mulib_clickir_clone (&c.program, 10000, &copy, NULL) == MULIB_MIR_OK);
  assert (copy.graph.nodes != c.graph.nodes);
  assert (mulib_clickir_to_flow (&c.program, 10000, &f, NULL) == MULIB_MIR_OK);
  assert (mulib_mir_frame_init (&frame, &f.program, NULL, 0, NULL, NULL, 10000, NULL) == MULIB_MIR_OK);
  frame.remaining = 1000;
  while (mulib_mir_frame_step (&frame) == MULIB_MIR_OK) {}
  assert (frame.status == MULIB_MIR_STEP_LIMIT); /* intentional infinite reset loop */
  mulib_mir_frame_free (&frame); mulib_clickir_flow_free (&f);
  mulib_clickir_free (&c); mulib_clickir_free (&copy); mulib_mir_native_free (&n);
}
static void unordered (void)
{
  const size_t none = MULIB_MIR_NONE;
  mulib_gssair_node_t nodes[] = {
    {MULIB_MIR_NATIVE_ADD, MULIB_MIR_I64, 0, 1, 2, 0, MULIB_MIR_NONE},
    {MULIB_MIR_NATIVE_CONST, MULIB_MIR_I64, 0, MULIB_MIR_NONE, MULIB_MIR_NONE, 20, MULIB_MIR_NONE},
    {MULIB_MIR_NATIVE_CONST, MULIB_MIR_I64, 0, MULIB_MIR_NONE, MULIB_MIR_NONE, 22, MULIB_MIR_NONE}
  };
  mulib_gssair_block_t bb = {MULIB_MIR_NATIVE_RETURN, 0, MULIB_MIR_NONE, MULIB_MIR_NONE};
  mulib_clickir_edge_t edge = {1, 2};
  mulib_clickir_program_t p = {{0}, &edge, 1};
  mulib_clickir_buffer_t c = {0};
  p.graph.nodes = nodes; p.graph.node_count = 3; p.graph.blocks = &bb;
  p.graph.block_count = 1; p.graph.signature.result = MULIB_MIR_I64;
  assert (mulib_clickir_validate (&p, 1000, NULL) == MULIB_MIR_OK);
  assert (mulib_clickir_clone (&p, 1000, &c, NULL) == MULIB_MIR_OK);
  assert (c.edges && c.edges != &edge && c.edges[0].before == 1);
  mulib_clickir_free (&c);
  edge.before = edge.after = 1;
  assert (mulib_clickir_validate (&p, 1000, NULL) == MULIB_MIR_BAD_REFERENCE);
  edge.before = 0; edge.after = 1;
  assert (mulib_clickir_validate (&p, 1000, NULL) == MULIB_MIR_BAD_REFERENCE);
  edge.before = none;
  assert (mulib_clickir_validate (&p, 1000, NULL) == MULIB_MIR_BAD_REFERENCE);
}
int main (void) { registers (); unordered (); return 0; }
