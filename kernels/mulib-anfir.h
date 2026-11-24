/** @file mulib-anfir.h
 * @brief A-normal let bindings with literal atoms and block-parameter joins.
 * Define MULIB_IMPL_ANFIR, MULIB_IMPL_GSSAIR and MULIB_IMPL_MIR.
 */
#ifndef MULIB_ANFIR_H
#define MULIB_ANFIR_H
#include "mulib-gssair.h"
#ifdef __cplusplus
extern "C" {
#endif
/** A let-bound operation. literal_mask bits 0/1 replace a/b with typed literal
 * atoms. Otherwise operands name bindings. memory must be NONE: ANF sequences
 * effects by binding order. PHIs represent block parameters. */
typedef struct mulib_anfir_binding {
  mulib_gssair_node_t value;
  unsigned literal_mask;
  uint64_t literals[2];
  mulib_mir_type_t literal_types[2];
} mulib_anfir_binding_t;
/** graph carries blocks, signatures, PHI inputs and call arguments indexed by
 * binding ID. graph.nodes must be NULL; graph.node_count is the binding count. */
typedef struct mulib_anfir_program {
  const mulib_anfir_binding_t *bindings;
  mulib_gssair_program_t graph;
} mulib_anfir_program_t;
typedef struct mulib_anfir_buffer {
  mulib_anfir_binding_t *bindings;
  mulib_gssair_buffer_t storage;
  mulib_anfir_program_t program;
} mulib_anfir_buffer_t;
/** Materialize literal atoms, remap named bindings and insert memory tokens. */
mulib_mir_status_t mulib_anfir_to_gssair (const mulib_anfir_program_t *program,
    size_t limit, mulib_gssair_buffer_t *out, mulib_mir_diagnostic_t *diagnostic);
/** Derive named atom-only bindings from validated SSA. Metadata is borrowed. */
mulib_mir_status_t mulib_anfir_from_gssair (const mulib_gssair_program_t *program,
    size_t limit, mulib_anfir_buffer_t *out, mulib_mir_diagnostic_t *diagnostic);
/** Normalize and lower to native MIR; failure preserves empty output. */
mulib_mir_status_t mulib_anfir_lower (const mulib_anfir_program_t *program,
    size_t limit, mulib_mir_native_buffer_t *out, mulib_mir_diagnostic_t *diagnostic);
/** Free binding/structural arrays, not borrowed signature metadata. */
void mulib_anfir_free (mulib_anfir_buffer_t *buffer);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ANFIR) && !defined(MULIB_ANFIR_IMPLEMENTED)
#define MULIB_ANFIR_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static mulib_mir_status_t
mulib_anfir_error_ (mulib_mir_diagnostic_t *d, mulib_mir_status_t s, size_t k)
{ if (d) { d->status = s; d->record = k; } return s; }
void mulib_anfir_free (mulib_anfir_buffer_t *b)
{
  if (!b) return;
  free (b->bindings); mulib_gssair_free (&b->storage); memset (b, 0, sizeof (*b));
}
mulib_mir_status_t
mulib_anfir_to_gssair (const mulib_anfir_program_t *p, size_t limit,
                     mulib_gssair_buffer_t *out, mulib_mir_diagnostic_t *d)
{
  mulib_gssair_buffer_t r;
  const mulib_gssair_program_t *g;
  size_t *map = NULL, *memory = NULL, k = 0, count, at = 0;
  mulib_mir_status_t s = MULIB_MIR_INVALID;
  memset (&r, 0, sizeof (r));
  if (!p || !p->bindings || !out || out->nodes || out->blocks || out->inputs
      || out->arguments || out->program.nodes || out->program.node_count) goto done;
  g = &p->graph;
  if (g->nodes || !g->node_count || !g->blocks || !g->block_count
      || (g->input_count && !g->inputs) || (g->argument_count && !g->arguments)) goto done;
  s = MULIB_MIR_RESOURCE_LIMIT;
  if (g->node_count > limit || g->block_count > limit || g->input_count > limit
      || g->argument_count > limit || g->node_count > SIZE_MAX / sizeof (*p->bindings)
      || g->block_count > SIZE_MAX / sizeof (*g->blocks)
      || g->input_count > SIZE_MAX / sizeof (*g->inputs)
      || g->argument_count > SIZE_MAX / sizeof (*g->arguments)) goto done;
  count = g->node_count;
  for (k = 0; k < g->node_count; ++k)
    {
      const mulib_anfir_binding_t *b = &p->bindings[k];
      int arity = mulib_mir_native_arity (b->value.opcode);
      unsigned bits = b->literal_mask;
      s = MULIB_MIR_INVALID;
      if (bits > 3 || (bits && arity < 1) || ((bits & 2) && arity != 2)
          || b->value.memory != MULIB_MIR_NONE) goto done;
      s = MULIB_MIR_RESOURCE_LIMIT;
      if ((size_t)((bits & 1) + ((bits >> 1) & 1)) > limit - count) goto done;
      count += (bits & 1) + ((bits >> 1) & 1);
    }
  if (count > SIZE_MAX / sizeof (*r.nodes)) goto done;
  map = (size_t *)malloc (g->node_count * sizeof (*map));
  memory = (size_t *)malloc (g->block_count * sizeof (*memory));
  r.nodes = (mulib_gssair_node_t *)malloc (count * sizeof (*r.nodes));
  r.blocks = (mulib_gssair_block_t *)malloc (g->block_count * sizeof (*r.blocks));
  if (g->input_count) r.inputs = (mulib_gssair_input_t *)malloc (g->input_count * sizeof (*r.inputs));
  if (g->argument_count) r.arguments = (size_t *)malloc (g->argument_count * sizeof (*r.arguments));
  s = MULIB_MIR_NO_MEMORY;
  if (!map || !memory || !r.nodes || !r.blocks || (g->input_count && !r.inputs)
      || (g->argument_count && !r.arguments)) goto done;
  for (k = 0; k < g->block_count; ++k) memory[k] = MULIB_MIR_NONE;
  for (k = 0; k < g->node_count; ++k)
    { unsigned bits = p->bindings[k].literal_mask; at += (bits & 1) + ((bits >> 1) & 1); map[k] = at++; }
  at = 0; s = MULIB_MIR_BAD_REFERENCE;
  for (k = 0; k < g->node_count; ++k)
    {
      const mulib_anfir_binding_t *b = &p->bindings[k];
      mulib_gssair_node_t n = b->value;
      int arity = mulib_mir_native_arity (n.opcode), j;
      if (n.block >= g->block_count) goto done;
      for (j = 0; j < arity; ++j)
        {
          size_t value = j ? n.b : n.a;
          if (b->literal_mask & (1u << j))
            {
              mulib_gssair_node_t literal = { MULIB_MIR_NATIVE_CONST, MULIB_MIR_I64,
                  0, MULIB_MIR_NONE, MULIB_MIR_NONE, 0, MULIB_MIR_NONE };
              literal.block = n.block; literal.type = b->literal_types[j];
              literal.immediate = b->literals[j]; value = at; r.nodes[at++] = literal;
            }
          else { if (value >= g->node_count) goto done; value = map[value]; }
          if (j) n.b = value; else n.a = value;
        }
      if (mulib_mir_native_effect (n.opcode))
        { n.memory = memory[n.block]; memory[n.block] = at; }
      r.nodes[at++] = n;
    }
  for (k = 0; k < g->block_count; ++k)
    {
      r.blocks[k] = g->blocks[k];
      if (r.blocks[k].value != MULIB_MIR_NONE)
        { if (r.blocks[k].value >= g->node_count) goto done; r.blocks[k].value = map[r.blocks[k].value]; }
    }
  for (k = 0; k < g->input_count; ++k)
    {
      r.inputs[k] = g->inputs[k];
      if (r.inputs[k].value >= g->node_count) goto done;
      r.inputs[k].value = map[r.inputs[k].value];
    }
  for (k = 0; k < g->argument_count; ++k)
    { if (g->arguments[k] >= g->node_count) goto done; r.arguments[k] = map[g->arguments[k]]; }
  r.program = *g; r.program.nodes = r.nodes; r.program.node_count = count;
  r.program.blocks = r.blocks; r.program.inputs = r.inputs; r.program.arguments = r.arguments;
  s = mulib_gssair_validate (&r.program, limit, d);
  if (s == MULIB_MIR_OK) { *out = r; memset (&r, 0, sizeof (r)); }
  free (map); free (memory); mulib_gssair_free (&r); return s;
done:
  free (map); free (memory); mulib_gssair_free (&r);
  return mulib_anfir_error_ (d, s, k);
}
mulib_mir_status_t
mulib_anfir_from_gssair (const mulib_gssair_program_t *p, size_t limit,
                       mulib_anfir_buffer_t *out, mulib_mir_diagnostic_t *d)
{
  mulib_anfir_buffer_t r;
  size_t k;
  mulib_mir_status_t s;
  memset (&r, 0, sizeof (r));
  if (!out || out->bindings || out->storage.nodes || out->program.bindings)
    return mulib_anfir_error_ (d, MULIB_MIR_INVALID, 0);
  s = mulib_gssair_validate (p, limit, d);
  if (s != MULIB_MIR_OK) return s;
  if (p->node_count > SIZE_MAX / sizeof (*r.bindings))
    return mulib_anfir_error_ (d, MULIB_MIR_RESOURCE_LIMIT, 0);
  s = mulib_gssair_permute (p, NULL, limit, &r.storage, d);
  if (s != MULIB_MIR_OK) return s;
  r.bindings = (mulib_anfir_binding_t *)calloc (p->node_count, sizeof (*r.bindings));
  if (!r.bindings) { mulib_anfir_free (&r); return mulib_anfir_error_ (d, MULIB_MIR_NO_MEMORY, 0); }
  for (k = 0; k < p->node_count; ++k)
    { r.bindings[k].value = p->nodes[k]; r.bindings[k].value.memory = MULIB_MIR_NONE; }
  r.program.graph = r.storage.program; r.program.graph.nodes = NULL;
  r.program.bindings = r.bindings; *out = r; return MULIB_MIR_OK;
}
mulib_mir_status_t
mulib_anfir_lower (const mulib_anfir_program_t *p, size_t limit,
                  mulib_mir_native_buffer_t *out, mulib_mir_diagnostic_t *d)
{
  mulib_gssair_buffer_t r;
  mulib_mir_status_t s;
  memset (&r, 0, sizeof (r));
  s = mulib_anfir_to_gssair (p, limit, &r, d);
  if (s == MULIB_MIR_OK) s = mulib_gssair_lower (&r.program, limit, out, d);
  mulib_gssair_free (&r); return s;
}
#ifdef __cplusplus
}
#endif
#endif
