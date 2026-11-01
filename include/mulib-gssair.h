/** @file mulib-gssair.h
 * @brief Gated, memory-ordered typed SSA with checked native MIR lowering.
 * Define MULIB_IMPL_GSSAIR and MULIB_IMPL_MIR in one translation unit.
 */
#ifndef MULIB_GSSAIR_H
#define MULIB_GSSAIR_H
#include "mulib-mir.h"
#ifdef __cplusplus
extern "C" {
#endif
enum { MULIB_GSSAIR_PHI = MULIB_MIR_NATIVE_OPCODE_COUNT };
/** Node ID is its array index. Native value opcodes use a/b as value IDs;
 * PARAM uses immediate as parameter index. CALL uses immediate as function
 * index and a/b as argument slice. PHI uses a/b as incoming-edge slice.
 * memory is the preceding LOAD/STORE/CALL in this block, or NONE at entry.
 * Other unused references are NONE and unused immediates zero. */
typedef struct mulib_gssair_node {
  int opcode;
  mulib_mir_type_t type;
  size_t block, a, b;
  uint64_t immediate;
  size_t memory;
} mulib_gssair_node_t;
/** A PHI input is gated by taking predecessor -> this block. */
typedef struct mulib_gssair_input { size_t predecessor, value; }
  mulib_gssair_input_t;
/** JUMP uses yes; BRANCH uses value/yes/no; RETURN uses value. */
typedef struct mulib_gssair_block {
  mulib_mir_native_opcode_t opcode;
  size_t value, yes, no;
} mulib_gssair_block_t;
typedef struct mulib_gssair_function {
  const char *name;
  mulib_mir_signature_t signature;
} mulib_gssair_function_t;
/** Borrowed graph. Nodes are grouped by ascending block, PHIs first, then
 * earlier local definitions. Entry is block zero, with no incoming edges.
 * Every block must be reachable. Types are I64, BOOL, WORD (address bits),
 * and VOID for STORE only. Arithmetic follows native MIR wrapping semantics. */
typedef struct mulib_gssair_program {
  const mulib_gssair_node_t *nodes;
  size_t node_count;
  const mulib_gssair_block_t *blocks;
  size_t block_count;
  const mulib_gssair_input_t *inputs;
  size_t input_count;
  const size_t *arguments;
  size_t argument_count;
  mulib_mir_signature_t signature;
  const mulib_gssair_function_t *functions;
  size_t function_count;
} mulib_gssair_program_t;
/** Owns structural arrays, borrows signature/function metadata from the input.
 * Zero initialize and free; never copy a live owner. */
typedef struct mulib_gssair_buffer {
  mulib_gssair_node_t *nodes;
  mulib_gssair_block_t *blocks;
  mulib_gssair_input_t *inputs;
  size_t *arguments;
  mulib_gssair_program_t program;
} mulib_gssair_buffer_t;
/** Check descriptors, types and reference ranges without requiring node order
 * or dominance; useful for graph transformations. limit bounds each array. */
mulib_mir_status_t mulib_gssair_shape (const mulib_gssair_program_t *program,
    size_t limit, mulib_mir_diagnostic_t *diagnostic);
/** Validate dominance, PHI predecessor sets, and the per-block memory chain.
 * The dense block_count squared analysis cells must also fit limit. */
mulib_mir_status_t mulib_gssair_validate (const mulib_gssair_program_t *program,
    size_t limit, mulib_mir_diagnostic_t *diagnostic);
/** Copy a shape-valid graph in order[new ID]=old ID (NULL means identity),
 * remapping all value references. Does not assert dominance or order. */
mulib_mir_status_t mulib_gssair_permute (const mulib_gssair_program_t *program,
    const size_t *order, size_t limit, mulib_gssair_buffer_t *out,
    mulib_mir_diagnostic_t *diagnostic);
/** Release owned arrays; borrowed metadata is not freed. */
void mulib_gssair_free (mulib_gssair_buffer_t *buffer);
/** Whether from has a CFG edge to to; callers must supply a valid graph. */
int mulib_gssair_edge (const mulib_gssair_program_t *program, size_t from,
    size_t to);
/** Lower SSA to mutable virtual registers. PHIs become parallel copies on
 * split edges, including critical edges and cyclic loop-carried copies.
 * Output owns all arrays and symbol names; failure leaves it empty. */
mulib_mir_status_t mulib_gssair_lower (const mulib_gssair_program_t *program,
    size_t limit, mulib_mir_native_buffer_t *out,
    mulib_mir_diagnostic_t *diagnostic);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_GSSAIR) && !defined(MULIB_GSSAIR_IMPLEMENTED)
#define MULIB_GSSAIR_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static mulib_mir_status_t
mulib_gssair_error_ (mulib_mir_diagnostic_t *d, mulib_mir_status_t s, size_t k)
{
  if (d) { d->status = s; d->record = k; }
  return s;
}
static int mulib_gssair_type_ (mulib_mir_type_t t)
{ return t == MULIB_MIR_I64 || t == MULIB_MIR_BOOL || t == MULIB_MIR_WORD; }
static mulib_mir_status_t
mulib_gssair_signature_ (const mulib_mir_signature_t *s, size_t limit)
{
  size_t k;
  if (s->parameter_count > limit || s->parameter_count > SIZE_MAX / sizeof (*s->parameters))
    return MULIB_MIR_RESOURCE_LIMIT;
  if (!mulib_gssair_type_ (s->result) || (s->parameter_count && !s->parameters))
    return MULIB_MIR_BAD_TYPE;
  for (k = 0; k < s->parameter_count; ++k)
    if (!mulib_gssair_type_ (s->parameters[k])) return MULIB_MIR_BAD_TYPE;
  return MULIB_MIR_OK;
}
int
mulib_gssair_edge (const mulib_gssair_program_t *p, size_t from, size_t to)
{
  const mulib_gssair_block_t *b = &p->blocks[from];
  return (b->opcode == MULIB_MIR_NATIVE_JUMP && b->yes == to)
      || (b->opcode == MULIB_MIR_NATIVE_BRANCH && (b->yes == to || b->no == to));
}
mulib_mir_status_t
mulib_gssair_shape (const mulib_gssair_program_t *p, size_t limit,
                    mulib_mir_diagnostic_t *d)
{
  size_t k = 0, j, total = 0;
  mulib_mir_status_t s = MULIB_MIR_INVALID;
  if (!p || !p->nodes || !p->node_count || !p->blocks || !p->block_count
      || (p->input_count && !p->inputs) || (p->argument_count && !p->arguments)
      || (p->function_count && !p->functions)) goto done;
  s = MULIB_MIR_RESOURCE_LIMIT;
  if (p->node_count > limit || p->block_count > limit || p->input_count > limit
      || p->argument_count > limit || p->function_count > limit
      || p->node_count > SIZE_MAX / sizeof (*p->nodes)
      || p->block_count > SIZE_MAX / sizeof (*p->blocks)
      || p->input_count > SIZE_MAX / sizeof (*p->inputs)
      || p->argument_count > SIZE_MAX / sizeof (*p->arguments)
      || p->function_count > SIZE_MAX / sizeof (*p->functions)) goto done;
  s = mulib_gssair_signature_ (&p->signature, limit);
  if (s != MULIB_MIR_OK) goto done;
  for (k = 0; k < p->function_count; ++k)
    {
      s = mulib_gssair_signature_ (&p->functions[k].signature, limit - total);
      if (s != MULIB_MIR_OK) goto done;
      total += p->functions[k].signature.parameter_count;
      s = MULIB_MIR_INVALID;
      if (!mulib_mir_native_name (p->functions[k].name)) goto done;
    }
  s = MULIB_MIR_BAD_REFERENCE;
  for (k = 0; k < p->input_count; ++k)
    if (p->inputs[k].predecessor >= p->block_count || p->inputs[k].value >= p->node_count)
      goto done;
  for (k = 0; k < p->argument_count; ++k)
    if (p->arguments[k] >= p->node_count) goto done;
  for (k = 0; k < p->node_count; ++k)
    {
      const mulib_gssair_node_t *n = &p->nodes[k];
      int arity = mulib_mir_native_arity (n->opcode);
      s = MULIB_MIR_BAD_OPCODE;
      if (n->opcode != MULIB_GSSAIR_PHI
          && (arity < 0 || n->opcode == MULIB_MIR_NATIVE_NOP
              || n->opcode >= MULIB_MIR_NATIVE_JUMP)) goto done;
      s = MULIB_MIR_BAD_REFERENCE;
      if (n->block >= p->block_count || (n->memory != MULIB_MIR_NONE
          && (n->memory >= p->node_count || !mulib_mir_native_effect (p->nodes[n->memory].opcode))))
        goto done;
      if (!mulib_mir_native_effect (n->opcode) && n->memory != MULIB_MIR_NONE) goto done;
      if (n->opcode == MULIB_GSSAIR_PHI || n->opcode == MULIB_MIR_NATIVE_CALL)
        {
          size_t count = n->opcode == MULIB_GSSAIR_PHI ? p->input_count : p->argument_count;
          if (n->a > count || n->b > count - n->a) goto done;
        }
      else if ((arity && n->a >= p->node_count) || (!arity && n->a != MULIB_MIR_NONE)
               || (arity == 2 && n->b >= p->node_count) || (arity < 2 && n->b != MULIB_MIR_NONE))
        goto done;
      s = MULIB_MIR_BAD_TYPE;
      if (n->opcode == MULIB_MIR_NATIVE_STORE ? n->type != MULIB_MIR_VOID : !mulib_gssair_type_ (n->type))
        goto done;
      if (n->opcode == MULIB_GSSAIR_PHI)
        {
          for (j = 0; j < n->b; ++j)
            if (p->nodes[p->inputs[n->a + j].value].type != n->type) goto done;
        }
      else if (n->opcode == MULIB_MIR_NATIVE_PARAM)
        {
          if (n->immediate >= p->signature.parameter_count)
            { s = MULIB_MIR_BAD_REFERENCE; goto done; }
          if (n->type != p->signature.parameters[(size_t)n->immediate]) goto done;
        }
      else if (n->opcode == MULIB_MIR_NATIVE_CALL)
        {
          const mulib_mir_signature_t *f;
          if (n->immediate >= p->function_count)
            { s = MULIB_MIR_BAD_REFERENCE; goto done; }
          f = &p->functions[(size_t)n->immediate].signature;
          if (n->type != f->result || n->b != f->parameter_count) goto done;
          for (j = 0; j < n->b; ++j)
            if (p->nodes[p->arguments[n->a + j]].type != f->parameters[j]) goto done;
        }
      else if (n->opcode == MULIB_MIR_NATIVE_CONST)
        { if (n->type == MULIB_MIR_BOOL && n->immediate > 1) goto done; }
      else
        {
          mulib_mir_type_t a = p->nodes[n->a].type;
          mulib_mir_type_t b = arity == 2 ? p->nodes[n->b].type : a;
          if (n->opcode == MULIB_MIR_NATIVE_LOAD)
            { if (a != MULIB_MIR_WORD || n->type == MULIB_MIR_BOOL) goto done; }
          else if (n->opcode == MULIB_MIR_NATIVE_STORE)
            { if (a != MULIB_MIR_WORD || !mulib_gssair_type_ (b)) goto done; }
          else if (n->opcode == MULIB_MIR_NATIVE_COPY)
            { if (a != n->type) goto done; }
          else if (n->opcode == MULIB_MIR_NATIVE_EQ)
            { if (a != b || !mulib_gssair_type_ (a) || n->type != MULIB_MIR_BOOL) goto done; }
          else if (n->opcode == MULIB_MIR_NATIVE_SLT || n->opcode == MULIB_MIR_NATIVE_ULT)
            {
              if (a != b || (a != MULIB_MIR_I64 && a != MULIB_MIR_WORD)
                  || n->type != MULIB_MIR_BOOL) goto done;
            }
          else if (a != b || a != n->type || (a != MULIB_MIR_I64 && a != MULIB_MIR_WORD)) goto done;
        }
      s = MULIB_MIR_INVALID;
      if (n->opcode != MULIB_MIR_NATIVE_CONST && n->opcode != MULIB_MIR_NATIVE_PARAM
          && n->opcode != MULIB_MIR_NATIVE_CALL && n->immediate) goto done;
    }
  for (k = 0; k < p->block_count; ++k)
    {
      const mulib_gssair_block_t *b = &p->blocks[k];
      s = MULIB_MIR_BAD_OPCODE;
      if (b->opcode != MULIB_MIR_NATIVE_JUMP && b->opcode != MULIB_MIR_NATIVE_BRANCH
          && b->opcode != MULIB_MIR_NATIVE_RETURN) goto done;
      s = MULIB_MIR_BAD_REFERENCE;
      if (b->opcode == MULIB_MIR_NATIVE_JUMP ? b->value != MULIB_MIR_NONE : b->value >= p->node_count)
        goto done;
      if (b->opcode == MULIB_MIR_NATIVE_RETURN ? b->yes != MULIB_MIR_NONE : b->yes >= p->block_count)
        goto done;
      if (b->opcode == MULIB_MIR_NATIVE_BRANCH ? b->no >= p->block_count : b->no != MULIB_MIR_NONE)
        goto done;
      s = MULIB_MIR_BAD_TYPE;
      if (b->opcode != MULIB_MIR_NATIVE_JUMP && p->nodes[b->value].type
          != (b->opcode == MULIB_MIR_NATIVE_BRANCH ? MULIB_MIR_BOOL : p->signature.result)) goto done;
    }
  s = MULIB_MIR_OK;
done:
  return mulib_gssair_error_ (d, s, k);
}
static int
mulib_gssair_dominates_ (const mulib_gssair_program_t *p, const unsigned char *dom,
                        size_t value, size_t block, size_t before)
{
  size_t b = p->nodes[value].block;
  return b == block ? value < before : dom[block * p->block_count + b] != 0;
}
mulib_mir_status_t
mulib_gssair_validate (const mulib_gssair_program_t *p, size_t limit,
                       mulib_mir_diagnostic_t *d)
{
  unsigned char *dom = NULL, *reachable = NULL;
  size_t k = 0, b, j, t, cells, last = MULIB_MIR_NONE;
  int changed, body = 0;
  mulib_mir_status_t s = mulib_gssair_shape (p, limit, d);
  if (s != MULIB_MIR_OK) return s;
  s = MULIB_MIR_RESOURCE_LIMIT;
  if (p->block_count > limit / p->block_count) goto done;
  cells = p->block_count * p->block_count;
  dom = (unsigned char *)malloc (cells);
  reachable = (unsigned char *)calloc (p->block_count, 1);
  s = MULIB_MIR_NO_MEMORY;
  if (!dom || !reachable) goto done;
  reachable[0] = 1;
  do {
    changed = 0;
    for (b = 0; b < p->block_count; ++b)
      for (j = 0; j < p->block_count; ++j)
        if (reachable[b] && mulib_gssair_edge (p, b, j) && !reachable[j])
          { reachable[j] = 1; changed = 1; }
  } while (changed);
  s = MULIB_MIR_BAD_REFERENCE;
  for (k = 0; k < p->block_count; ++k)
    if (!reachable[k] || mulib_gssair_edge (p, k, 0)) goto done;
  memset (dom, 1, cells); memset (dom, 0, p->block_count); dom[0] = 1;
  do {
    changed = 0;
    for (b = 1; b < p->block_count; ++b)
      for (j = 0; j < p->block_count; ++j)
        if (j != b && dom[b * p->block_count + j])
          for (t = 0; t < p->block_count; ++t)
            if (mulib_gssair_edge (p, t, b) && !dom[t * p->block_count + j])
              { dom[b * p->block_count + j] = 0; changed = 1; break; }
  } while (changed);
  for (k = 0; k < p->node_count; ++k)
    {
      const mulib_gssair_node_t *n = &p->nodes[k];
      b = n->block;
      if (k && b < p->nodes[k - 1].block) goto done;
      if (!k || b != p->nodes[k - 1].block) { last = MULIB_MIR_NONE; body = 0; }
      if (n->opcode == MULIB_GSSAIR_PHI)
        {
          size_t predecessors = 0;
          if (body || !b) goto done;
          for (j = 0; j < p->block_count; ++j)
            if (mulib_gssair_edge (p, j, b)) ++predecessors;
          if (n->b != predecessors) goto done;
          for (j = 0; j < n->b; ++j)
            {
              const mulib_gssair_input_t *in = &p->inputs[n->a + j];
              if (!mulib_gssair_edge (p, in->predecessor, b)
                  || !mulib_gssair_dominates_ (p, dom, in->value, in->predecessor, p->node_count)) goto done;
              for (t = 0; t < j; ++t)
                if (p->inputs[n->a + t].predecessor == in->predecessor) goto done;
            }
        }
      else
        {
          int arity = mulib_mir_native_arity (n->opcode);
          body = 1;
          if ((arity && !mulib_gssair_dominates_ (p, dom, n->a, b, k))
              || (arity == 2 && !mulib_gssair_dominates_ (p, dom, n->b, b, k))) goto done;
          if (n->opcode == MULIB_MIR_NATIVE_CALL)
            for (j = 0; j < n->b; ++j)
              if (!mulib_gssair_dominates_ (p, dom, p->arguments[n->a + j], b, k)) goto done;
          if (mulib_mir_native_effect (n->opcode))
            { if (n->memory != last) goto done; last = k; }
        }
    }
  for (k = 0; k < p->block_count; ++k)
    if (p->blocks[k].opcode != MULIB_MIR_NATIVE_JUMP
        && !mulib_gssair_dominates_ (p, dom, p->blocks[k].value, k, p->node_count)) goto done;
  s = MULIB_MIR_OK;
done:
  free (dom); free (reachable);
  return mulib_gssair_error_ (d, s, k);
}
void mulib_gssair_free (mulib_gssair_buffer_t *b)
{
  if (!b) return;
  free (b->nodes); free (b->blocks); free (b->inputs); free (b->arguments);
  memset (b, 0, sizeof (*b));
}
mulib_mir_status_t
mulib_gssair_permute (const mulib_gssair_program_t *p, const size_t *order,
                     size_t limit, mulib_gssair_buffer_t *out, mulib_mir_diagnostic_t *d)
{
  mulib_gssair_buffer_t b;
  size_t *map = NULL, k = 0;
  mulib_mir_status_t s = mulib_gssair_shape (p, limit, d);
  if (s != MULIB_MIR_OK) return s;
  memset (&b, 0, sizeof (b));
  s = MULIB_MIR_INVALID;
  if (!out || out->nodes || out->blocks || out->inputs || out->arguments
      || out->program.node_count || out->program.nodes) goto done;
  map = (size_t *)malloc (p->node_count * sizeof (*map));
  b.nodes = (mulib_gssair_node_t *)malloc (p->node_count * sizeof (*b.nodes));
  b.blocks = (mulib_gssair_block_t *)malloc (p->block_count * sizeof (*b.blocks));
  if (p->input_count) b.inputs = (mulib_gssair_input_t *)malloc (p->input_count * sizeof (*b.inputs));
  if (p->argument_count) b.arguments = (size_t *)malloc (p->argument_count * sizeof (*b.arguments));
  s = MULIB_MIR_NO_MEMORY;
  if (!map || !b.nodes || !b.blocks || (p->input_count && !b.inputs)
      || (p->argument_count && !b.arguments)) goto done;
  for (k = 0; k < p->node_count; ++k) map[k] = MULIB_MIR_NONE;
  s = MULIB_MIR_BAD_REFERENCE;
  for (k = 0; k < p->node_count; ++k)
    {
      size_t old = order ? order[k] : k;
      if (old >= p->node_count || map[old] != MULIB_MIR_NONE) goto done;
      map[old] = k;
    }
  for (k = 0; k < p->node_count; ++k)
    {
      mulib_gssair_node_t n = p->nodes[order ? order[k] : k];
      int arity = mulib_mir_native_arity (n.opcode);
      if (arity > 0) n.a = map[n.a];
      if (arity == 2) n.b = map[n.b];
      if (n.memory != MULIB_MIR_NONE) n.memory = map[n.memory];
      b.nodes[k] = n;
    }
  for (k = 0; k < p->block_count; ++k)
    { b.blocks[k] = p->blocks[k]; if (b.blocks[k].value != MULIB_MIR_NONE) b.blocks[k].value = map[b.blocks[k].value]; }
  for (k = 0; k < p->input_count; ++k)
    { b.inputs[k] = p->inputs[k]; b.inputs[k].value = map[b.inputs[k].value]; }
  for (k = 0; k < p->argument_count; ++k) b.arguments[k] = map[p->arguments[k]];
  b.program = *p; b.program.nodes = b.nodes; b.program.blocks = b.blocks;
  b.program.inputs = b.inputs; b.program.arguments = b.arguments;
  *out = b; memset (&b, 0, sizeof (b)); s = MULIB_MIR_OK;
done:
  free (map); mulib_gssair_free (&b);
  return mulib_gssair_error_ (d, s, k);
}
mulib_mir_status_t
mulib_gssair_lower (const mulib_gssair_program_t *p, size_t limit,
                   mulib_mir_native_buffer_t *out, mulib_mir_diagnostic_t *d)
{
  mulib_mir_native_buffer_t r;
  size_t *edge = NULL, *temp = NULL;
  size_t k = 0, b, j, t, phis = 0, nb, count, pc = 0, inputs = 0;
  mulib_mir_status_t s = mulib_gssair_validate (p, limit, d);
  if (s != MULIB_MIR_OK) return s;
  memset (&r, 0, sizeof (r));
  s = MULIB_MIR_INVALID;
  if (!out || out->code || out->blocks || out->arguments || out->symbols
      || out->count || out->block_count || out->register_count || out->parameter_count
      || out->argument_count || out->symbol_count) goto done;
  s = MULIB_MIR_RESOURCE_LIMIT;
  if (p->block_count * p->block_count > SIZE_MAX / sizeof (*edge)) goto done;
  edge = (size_t *)malloc (p->block_count * p->block_count * sizeof (*edge));
  temp = (size_t *)malloc (p->node_count * sizeof (*temp));
  s = MULIB_MIR_NO_MEMORY;
  if (!edge || !temp) goto done;
  nb = p->block_count;
  for (k = 0; k < p->node_count; ++k)
    if (p->nodes[k].opcode == MULIB_GSSAIR_PHI)
      {
        s = MULIB_MIR_RESOURCE_LIMIT;
        if (phis >= limit - p->node_count || p->nodes[k].b > limit - inputs) goto done;
        inputs += p->nodes[k].b; temp[k] = p->node_count + phis++;
      }
  for (b = 0; b < p->block_count; ++b)
    for (j = 0; j < p->block_count; ++j)
      {
        edge[b * p->block_count + j] = j;
        if (!mulib_gssair_edge (p, b, j)) continue;
        for (k = 0; k < p->node_count; ++k)
          if (p->nodes[k].block == j && p->nodes[k].opcode == MULIB_GSSAIR_PHI) break;
        if (k < p->node_count)
          {
            s = MULIB_MIR_RESOURCE_LIMIT;
            if (nb == limit) goto done;
            edge[b * p->block_count + j] = nb++;
          }
      }
  count = p->node_count - phis;
  s = MULIB_MIR_RESOURCE_LIMIT;
  if (nb > limit - count) goto done;
  count += nb;
  if (inputs > (limit - count) / 2) goto done;
  count += inputs * 2;
  s = mulib_mir_native_init (&r, count, nb, p->node_count + phis,
      p->signature.parameter_count, p->argument_count, p->function_count, limit);
  if (s != MULIB_MIR_OK) goto done;
  for (k = 0; k < p->function_count; ++k)
    if ((s = mulib_mir_native_symbol (&r, k, p->functions[k].name)) != MULIB_MIR_OK) goto done;
  if (p->argument_count) memcpy (r.arguments, p->arguments, p->argument_count * sizeof (*r.arguments));
  for (b = 0; b < p->block_count; ++b)
    {
      mulib_mir_native_instruction_t i;
      const mulib_gssair_block_t *bb = &p->blocks[b];
      r.blocks[b].first = pc;
      for (k = 0; k < p->node_count; ++k)
        {
          const mulib_gssair_node_t *n = &p->nodes[k];
          if (n->block != b || n->opcode == MULIB_GSSAIR_PHI) continue;
          i = mulib_mir_native_instruction ((mulib_mir_native_opcode_t)n->opcode);
          if (mulib_mir_native_defines (n->opcode)) i.destination = k;
          i.a = n->a; i.b = n->b;
          if (n->opcode == MULIB_MIR_NATIVE_CONST) i.immediate = n->immediate;
          if (n->opcode == MULIB_MIR_NATIVE_PARAM) i.a = (size_t)n->immediate;
          if (n->opcode == MULIB_MIR_NATIVE_CALL) i.target = (size_t)n->immediate;
          r.code[pc++] = i;
        }
      i = mulib_mir_native_instruction (bb->opcode); i.a = bb->value;
      if (bb->yes != MULIB_MIR_NONE) i.target = edge[b * p->block_count + bb->yes];
      if (bb->no != MULIB_MIR_NONE) i.b = edge[b * p->block_count + bb->no];
      r.code[pc++] = i; r.blocks[b].count = pc - r.blocks[b].first;
    }
  for (b = 0; b < p->block_count; ++b)
    for (j = 0; j < p->block_count; ++j)
      if (edge[b * p->block_count + j] >= p->block_count)
        {
          size_t id = edge[b * p->block_count + j];
          mulib_mir_native_instruction_t i;
          r.blocks[id].first = pc;
          for (k = 0; k < p->node_count; ++k)
            if (p->nodes[k].block == j && p->nodes[k].opcode == MULIB_GSSAIR_PHI)
              {
                const mulib_gssair_node_t *n = &p->nodes[k];
                for (t = 0; t < n->b; ++t)
                  if (p->inputs[n->a + t].predecessor == b) break;
                i = mulib_mir_native_instruction (MULIB_MIR_NATIVE_COPY);
                i.destination = temp[k]; i.a = p->inputs[n->a + t].value;
                r.code[pc++] = i;
              }
          for (k = 0; k < p->node_count; ++k)
            if (p->nodes[k].block == j && p->nodes[k].opcode == MULIB_GSSAIR_PHI)
              {
                i = mulib_mir_native_instruction (MULIB_MIR_NATIVE_COPY);
                i.destination = k; i.a = temp[k]; r.code[pc++] = i;
              }
          i = mulib_mir_native_instruction (MULIB_MIR_NATIVE_JUMP);
          i.target = j; r.code[pc++] = i; r.blocks[id].count = pc - r.blocks[id].first;
        }
  s = mulib_mir_native_validate (&(mulib_mir_native_program_t){r.code,r.count,r.blocks,
      r.block_count,r.register_count,r.parameter_count,r.arguments,r.argument_count,
      (const char *const *)r.symbols,r.symbol_count}, limit, d);
  if (s == MULIB_MIR_OK) { *out = r; memset (&r, 0, sizeof (r)); }
done:
  free (edge); free (temp); mulib_mir_native_free (&r);
  return mulib_gssair_error_ (d, s, k);
}
#ifdef __cplusplus
}
#endif
#endif
