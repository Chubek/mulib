/** @file mulib-clickir.h
 * @brief Block-pinned Sea of Nodes with data, memory and precedence edges.
 * Define MULIB_IMPL_CLICKIR, MULIB_IMPL_GSSAIR and MULIB_IMPL_MIR.
 */
#ifndef MULIB_CLICKIR_H
#define MULIB_CLICKIR_H
#include "mulib-gssair.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_clickir_edge { size_t before, after; } mulib_clickir_edge_t;
/** Nodes may appear in any array order. Block IDs pin execution; PHIs encode
 * control merges. Additional edges order nodes within one block. Memory edges
 * must still form a complete chain of memory/trapping operations per block. */
typedef struct mulib_clickir_program {
  mulib_gssair_program_t graph;
  const mulib_clickir_edge_t *edges;
  size_t edge_count;
} mulib_clickir_program_t;
/** Owns graph structure and additional edges; borrows signature metadata. */
typedef struct mulib_clickir_buffer {
  mulib_gssair_buffer_t graph;
  mulib_clickir_edge_t *edges;
  mulib_clickir_program_t program;
} mulib_clickir_buffer_t;
/** Topologically order each pinned block, then validate SSA and memory gates.
 * Cross-block data cycles are legal only through PHIs. Failure is atomic. */
mulib_mir_status_t mulib_clickir_to_gssair (const mulib_clickir_program_t *program,
    size_t limit, mulib_gssair_buffer_t *out, mulib_mir_diagnostic_t *diagnostic);
/** Copy a validated SSA graph into an owned Sea of Nodes. */
mulib_mir_status_t mulib_clickir_from_gssair (const mulib_gssair_program_t *program,
    size_t limit, mulib_clickir_buffer_t *out, mulib_mir_diagnostic_t *diagnostic);
/** Normalize and lower to native MIR, including edge-local parallel PHI copies. */
mulib_mir_status_t mulib_clickir_lower (const mulib_clickir_program_t *program,
    size_t limit, mulib_mir_native_buffer_t *out, mulib_mir_diagnostic_t *diagnostic);
/** Free owned arrays, retaining no borrowed references. */
void mulib_clickir_free (mulib_clickir_buffer_t *buffer);
/** Validate unordered nodes and precedence, without retaining a normalized copy. */
mulib_mir_status_t mulib_clickir_validate (const mulib_clickir_program_t *program,
    size_t limit, mulib_mir_diagnostic_t *diagnostic);
/** Deep-copy structure and precedence, borrowing signature metadata. */
mulib_mir_status_t mulib_clickir_clone (const mulib_clickir_program_t *program,
    size_t limit, mulib_clickir_buffer_t *out, mulib_mir_diagnostic_t *diagnostic);
/** Import typed mutable native registers, inserting a synthetic entry, zero
 * initial values and parallel PHIs at every original block. Types must be fixed
 * per register. All original blocks must be reachable. Metadata is borrowed.
 * limit also bounds block/register cells and generated nodes/PHI inputs. */
mulib_mir_status_t mulib_clickir_from_native (const mulib_mir_native_program_t *program,
    const mulib_mir_type_t *types, mulib_mir_signature_t signature, size_t limit,
    mulib_clickir_buffer_t *out, mulib_mir_diagnostic_t *diagnostic);
/** Owned executable integer MIR, borrowing only signature parameter metadata.
 * Zero initialize; no live owner copies. Memory, calls and WORD are unsupported. */
typedef struct mulib_clickir_flow {
  mulib_mir_flow_instruction_t *code;
  mulib_mir_type_t *types;
  mulib_mir_flow_program_t program;
} mulib_clickir_flow_t;
/** Lower I64/BOOL graphs to safe, budgeted executable MIR. Native wrapping,
 * masked shifts and signed/unsigned comparisons are preserved. SDIV/SREM trap
 * on zero and INT64_MIN/-1. Failure is atomic. */
mulib_mir_status_t mulib_clickir_to_flow (const mulib_clickir_program_t *program,
    size_t limit, mulib_clickir_flow_t *out, mulib_mir_diagnostic_t *diagnostic);
/** Release executable MIR arrays. */
void mulib_clickir_flow_free (mulib_clickir_flow_t *buffer);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CLICKIR) && !defined(MULIB_CLICKIR_IMPLEMENTED)
#define MULIB_CLICKIR_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static mulib_mir_status_t
mulib_clickir_error_ (mulib_mir_diagnostic_t *d, mulib_mir_status_t s, size_t k)
{ if (d) { d->status = s; d->record = k; } return s; }
static int mulib_clickir_empty_ (const mulib_clickir_buffer_t *b)
{
  return b && !b->edges && !b->graph.nodes && !b->graph.blocks
    && !b->graph.inputs && !b->graph.arguments && !b->graph.program.nodes
    && !b->graph.program.node_count && !b->program.graph.nodes
    && !b->program.graph.node_count && !b->program.edges && !b->program.edge_count;
}
void mulib_clickir_free (mulib_clickir_buffer_t *b)
{
  if (!b) return;
  mulib_gssair_free (&b->graph); free (b->edges); memset (b, 0, sizeof (*b));
}
mulib_mir_status_t
mulib_clickir_to_gssair (const mulib_clickir_program_t *p, size_t limit,
                       mulib_gssair_buffer_t *out, mulib_mir_diagnostic_t *d)
{
  mulib_gssair_buffer_t r;
  unsigned char *done = NULL;
  size_t *order = NULL, k = 0, b, j, count = 0;
  const mulib_gssair_program_t *g;
  mulib_mir_status_t s;
  memset (&r, 0, sizeof (r));
  if (!p || (p->edge_count && !p->edges) || !out || out->nodes || out->blocks
      || out->inputs || out->arguments || out->program.nodes || out->program.node_count)
    return mulib_clickir_error_ (d, MULIB_MIR_INVALID, 0);
  g = &p->graph;
  s = mulib_gssair_shape (g, limit, d);
  if (s != MULIB_MIR_OK) return s;
  s = MULIB_MIR_RESOURCE_LIMIT;
  if (p->edge_count > limit || p->edge_count > SIZE_MAX / sizeof (*p->edges)) goto finish;
  s = MULIB_MIR_BAD_REFERENCE;
  for (k = 0; k < p->edge_count; ++k)
    if (p->edges[k].before >= g->node_count || p->edges[k].after >= g->node_count
        || p->edges[k].before == p->edges[k].after
        || g->nodes[p->edges[k].before].block != g->nodes[p->edges[k].after].block) goto finish;
  order = (size_t *)malloc (g->node_count * sizeof (*order));
  done = (unsigned char *)calloc (g->node_count, 1);
  s = MULIB_MIR_NO_MEMORY;
  if (!order || !done) goto finish;
  for (b = 0; b < g->block_count; ++b)
    {
      size_t remaining = 0;
      for (k = 0; k < g->node_count; ++k) if (g->nodes[k].block == b) ++remaining;
      while (remaining)
        {
          size_t best = MULIB_MIR_NONE;
          for (k = 0; k < g->node_count; ++k)
            {
              const mulib_gssair_node_t *n = &g->nodes[k];
              int arity = mulib_mir_native_arity (n->opcode), ready = 1;
              if (done[k] || n->block != b) continue;
              if (arity > 0 && g->nodes[n->a].block == b && !done[n->a]) ready = 0;
              if (arity == 2 && g->nodes[n->b].block == b && !done[n->b]) ready = 0;
               if (n->memory != MULIB_MIR_NONE
                   && (g->nodes[n->memory].block != b || !done[n->memory])) ready = 0;
              if (n->opcode == MULIB_MIR_NATIVE_CALL)
                for (j = 0; j < n->b; ++j)
                  if (g->nodes[g->arguments[n->a + j]].block == b && !done[g->arguments[n->a + j]]) ready = 0;
              for (j = 0; j < p->edge_count; ++j)
                if (p->edges[j].after == k && !done[p->edges[j].before]) ready = 0;
              if (ready && (best == MULIB_MIR_NONE || n->opcode == MULIB_GSSAIR_PHI))
                { best = k; if (n->opcode == MULIB_GSSAIR_PHI) break; }
            }
          s = MULIB_MIR_BAD_REFERENCE;
          if (best == MULIB_MIR_NONE) { k = b; goto finish; }
          order[count++] = best; done[best] = 1; --remaining;
        }
    }
  s = mulib_gssair_permute (g, order, limit, &r, d);
  if (s == MULIB_MIR_OK) s = mulib_gssair_validate (&r.program, limit, d);
  if (s == MULIB_MIR_OK) { *out = r; memset (&r, 0, sizeof (r)); }
  free (done); free (order); mulib_gssair_free (&r);
  return s;
finish:
  free (done); free (order); mulib_gssair_free (&r);
  return mulib_clickir_error_ (d, s, k);
}
mulib_mir_status_t
mulib_clickir_from_gssair (const mulib_gssair_program_t *p, size_t limit,
                         mulib_clickir_buffer_t *out, mulib_mir_diagnostic_t *d)
{
  mulib_clickir_buffer_t r;
  mulib_mir_status_t s;
  memset (&r, 0, sizeof (r));
   if (!mulib_clickir_empty_ (out))
    return mulib_clickir_error_ (d, MULIB_MIR_INVALID, 0);
  s = mulib_gssair_validate (p, limit, d);
  if (s == MULIB_MIR_OK) s = mulib_gssair_permute (p, NULL, limit, &r.graph, d);
  if (s == MULIB_MIR_OK) { r.program.graph = r.graph.program; *out = r; }
  return s;
}
mulib_mir_status_t
mulib_clickir_validate (const mulib_clickir_program_t *p, size_t limit,
                       mulib_mir_diagnostic_t *d)
{
  mulib_gssair_buffer_t b;
  mulib_mir_status_t s;
  memset (&b, 0, sizeof (b));
  s = mulib_clickir_to_gssair (p, limit, &b, d);
  mulib_gssair_free (&b); return s;
}
mulib_mir_status_t
mulib_clickir_clone (const mulib_clickir_program_t *p, size_t limit,
                    mulib_clickir_buffer_t *out, mulib_mir_diagnostic_t *d)
{
  mulib_clickir_buffer_t b;
  mulib_mir_status_t s;
  if (!mulib_clickir_empty_ (out)) return mulib_clickir_error_ (d, MULIB_MIR_INVALID, 0);
  memset (&b, 0, sizeof (b));
  s = mulib_clickir_validate (p, limit, d);
  if (s != MULIB_MIR_OK) return s;
  s = mulib_gssair_permute (&p->graph, NULL, limit, &b.graph, d);
  if (s != MULIB_MIR_OK) return s;
  if (p->edge_count)
    {
      b.edges = (mulib_clickir_edge_t *)malloc (p->edge_count * sizeof (*b.edges));
      if (!b.edges) { mulib_clickir_free (&b); return mulib_clickir_error_ (d, MULIB_MIR_NO_MEMORY, 0); }
      memcpy (b.edges, p->edges, p->edge_count * sizeof (*b.edges));
    }
  b.program.graph = b.graph.program; b.program.edges = b.edges;
  b.program.edge_count = p->edge_count; *out = b;
  return mulib_clickir_error_ (d, MULIB_MIR_OK, 0);
}
static mulib_gssair_node_t
mulib_clickir_node_ (int op, mulib_mir_type_t type, size_t block)
{
  mulib_gssair_node_t n;
  memset (&n, 0, sizeof (n)); n.opcode = op; n.type = type; n.block = block;
  n.a = n.b = n.memory = MULIB_MIR_NONE; return n;
}
mulib_mir_status_t
mulib_clickir_from_native (const mulib_mir_native_program_t *p,
    const mulib_mir_type_t *types, mulib_mir_signature_t signature, size_t limit,
    mulib_clickir_buffer_t *out, mulib_mir_diagnostic_t *d)
{
  mulib_clickir_buffer_t r;
  size_t *exits = NULL, *current = NULL, *first = NULL;
  size_t k = 0, b, j, v, cells, nodes, inputs, edges = 1, pc = 0, ip = 0;
  mulib_mir_status_t s;
  memset (&r, 0, sizeof (r));
  if (!mulib_clickir_empty_ (out) || !types || !p || !p->register_count
      || signature.parameter_count != p->parameter_count || p->symbol_count)
    return mulib_clickir_error_ (d, MULIB_MIR_INVALID, 0);
  s = mulib_mir_native_validate (p, limit, d);
  if (s != MULIB_MIR_OK) return s;
  s = MULIB_MIR_RESOURCE_LIMIT;
  if (p->block_count >= limit || p->register_count > limit / p->block_count
      || p->block_count + 1 > limit / (p->block_count + 1)) goto done;
  cells = p->block_count * p->register_count;
  if (cells > SIZE_MAX / sizeof (*exits) || cells > limit - p->register_count) goto done;
  nodes = cells + p->register_count;
  for (k = 0; k < p->count; ++k)
    {
      const mulib_mir_native_instruction_t *i = &p->code[k];
      if (mulib_mir_native_defines (i->opcode)) { if (nodes == limit) goto done; ++nodes; }
      if (i->opcode == MULIB_MIR_NATIVE_JUMP) ++edges;
      if (i->opcode == MULIB_MIR_NATIVE_BRANCH) edges += i->target == i->b ? 1 : 2;
      if (i->opcode == MULIB_MIR_NATIVE_LOAD || i->opcode == MULIB_MIR_NATIVE_STORE
          || i->opcode == MULIB_MIR_NATIVE_CALL) { s = MULIB_MIR_UNSUPPORTED; goto done; }
    }
  if (edges > limit / p->register_count) goto done;
  inputs = edges * p->register_count;
  if (nodes > SIZE_MAX / sizeof (*r.graph.nodes)
      || inputs > SIZE_MAX / sizeof (*r.graph.inputs)
      || p->block_count + 1 > SIZE_MAX / sizeof (*r.graph.blocks)) goto done;
  for (k = 0; k < p->register_count; ++k)
    if (types[k] != MULIB_MIR_I64 && types[k] != MULIB_MIR_BOOL && types[k] != MULIB_MIR_WORD)
      { s = MULIB_MIR_BAD_TYPE; goto done; }
  r.graph.nodes = (mulib_gssair_node_t *)calloc (nodes, sizeof (*r.graph.nodes));
  r.graph.blocks = (mulib_gssair_block_t *)calloc (p->block_count + 1, sizeof (*r.graph.blocks));
  r.graph.inputs = (mulib_gssair_input_t *)malloc (inputs * sizeof (*r.graph.inputs));
  exits = (size_t *)malloc (cells * sizeof (*exits));
  current = (size_t *)malloc (p->register_count * sizeof (*current));
  first = (size_t *)malloc (p->block_count * sizeof (*first));
  s = MULIB_MIR_NO_MEMORY;
  if (!r.graph.nodes || !r.graph.blocks || !r.graph.inputs || !exits || !current || !first) goto done;
  r.graph.program.nodes = r.graph.nodes; r.graph.program.node_count = nodes;
  r.graph.program.blocks = r.graph.blocks; r.graph.program.block_count = p->block_count + 1;
  r.graph.program.inputs = r.graph.inputs; r.graph.program.input_count = inputs;
  r.graph.program.signature = signature;
  r.graph.blocks[0].opcode = MULIB_MIR_NATIVE_JUMP;
  r.graph.blocks[0].value = r.graph.blocks[0].no = MULIB_MIR_NONE; r.graph.blocks[0].yes = 1;
  for (v = 0; v < p->register_count; ++v)
    r.graph.nodes[pc++] = mulib_clickir_node_ (MULIB_MIR_NATIVE_CONST, types[v], 0);
  for (b = 0; b < p->block_count; ++b)
    {
      size_t last = MULIB_MIR_NONE;
      mulib_gssair_block_t *bb = &r.graph.blocks[b + 1];
      first[b] = pc;
      for (v = 0; v < p->register_count; ++v)
        { current[v] = pc; r.graph.nodes[pc++] = mulib_clickir_node_ (MULIB_GSSAIR_PHI, types[v], b + 1); }
      for (k = p->blocks[b].first; k < p->blocks[b].first + p->blocks[b].count; ++k)
        {
          const mulib_mir_native_instruction_t *i = &p->code[k];
          int arity = mulib_mir_native_arity (i->opcode);
          if (i->opcode >= MULIB_MIR_NATIVE_JUMP)
            {
              bb->opcode = i->opcode; bb->value = i->a == MULIB_MIR_NONE ? MULIB_MIR_NONE : current[i->a];
              bb->yes = i->target == MULIB_MIR_NONE ? MULIB_MIR_NONE : i->target + 1;
              bb->no = i->opcode == MULIB_MIR_NATIVE_BRANCH ? i->b + 1 : MULIB_MIR_NONE;
            }
          else if (i->opcode != MULIB_MIR_NATIVE_NOP)
            {
              mulib_gssair_node_t n = mulib_clickir_node_ (i->opcode, types[i->destination], b + 1);
              if (arity) n.a = current[i->a];
              if (arity == 2) n.b = current[i->b];
              if (i->opcode == MULIB_MIR_NATIVE_PARAM) n.immediate = i->a;
              if (i->opcode == MULIB_MIR_NATIVE_CONST) n.immediate = i->immediate;
              if (mulib_mir_native_effect (i->opcode)) { n.memory = last; last = pc; }
              current[i->destination] = pc; r.graph.nodes[pc++] = n;
            }
        }
      memcpy (exits + b * p->register_count, current, p->register_count * sizeof (*current));
    }
  for (b = 0; b < p->block_count; ++b)
    for (v = 0; v < p->register_count; ++v)
      {
        mulib_gssair_node_t *n = &r.graph.nodes[first[b] + v];
        n->a = ip;
        if (!b) { r.graph.inputs[ip].predecessor = 0; r.graph.inputs[ip++].value = v; }
        for (j = 0; j < p->block_count; ++j)
          if (mulib_gssair_edge (&r.graph.program, j + 1, b + 1))
            { r.graph.inputs[ip].predecessor = j + 1; r.graph.inputs[ip++].value = exits[j * p->register_count + v]; }
        n->b = ip - n->a;
      }
  s = mulib_gssair_validate (&r.graph.program, limit, d);
  if (s == MULIB_MIR_OK) { r.program.graph = r.graph.program; *out = r; memset (&r, 0, sizeof (r)); }
done:
  free (exits); free (current); free (first); mulib_clickir_free (&r);
  return mulib_clickir_error_ (d, s, k);
}
void mulib_clickir_flow_free (mulib_clickir_flow_t *b)
{ if (b) { free (b->code); free (b->types); memset (b, 0, sizeof (*b)); } }
static mulib_mir_flow_instruction_t mulib_clickir_flow_instruction_ (mulib_mir_flow_opcode_t op)
{
  mulib_mir_flow_instruction_t i;
  memset (&i, 0, sizeof (i)); i.opcode = op;
  i.destination = i.a = i.b = i.target = MULIB_MIR_NONE; return i;
}
mulib_mir_status_t
mulib_clickir_to_flow (const mulib_clickir_program_t *p, size_t limit,
                      mulib_clickir_flow_t *out, mulib_mir_diagnostic_t *d)
{
  mulib_gssair_buffer_t g;
  mulib_mir_native_buffer_t n;
  mulib_clickir_flow_t r;
  size_t *map = NULL, *blocks = NULL, k = 0, j, pc = 0, count, nr, phis = 0;
  mulib_mir_status_t s;
  memset (&g, 0, sizeof (g)); memset (&n, 0, sizeof (n)); memset (&r, 0, sizeof (r));
  if (!out || out->code || out->types || out->program.code || out->program.count)
    return mulib_clickir_error_ (d, MULIB_MIR_INVALID, 0);
  s = mulib_clickir_to_gssair (p, limit, &g, d);
  if (s != MULIB_MIR_OK) return s;
  s = MULIB_MIR_UNSUPPORTED;
  if (g.program.function_count) goto done;
  for (k = 0; k < g.program.node_count; ++k)
    if (g.nodes[k].type != MULIB_MIR_I64 && g.nodes[k].type != MULIB_MIR_BOOL) goto done;
  s = mulib_gssair_lower (&g.program, limit, &n, d);
  if (s != MULIB_MIR_OK) goto done;
  s = MULIB_MIR_RESOURCE_LIMIT;
  if (n.register_count > limit || limit - n.register_count < 3
      || n.register_count > SIZE_MAX / sizeof (*r.types) - 3 || n.count > limit / 5
      || n.count > SIZE_MAX / (5 * sizeof (*r.code))) goto done;
  nr = n.register_count; count = n.count * 5;
  r.code = (mulib_mir_flow_instruction_t *)calloc (count, sizeof (*r.code));
  r.types = (mulib_mir_type_t *)malloc ((nr + 3) * sizeof (*r.types));
  map = (size_t *)malloc (n.count * sizeof (*map));
  blocks = (size_t *)malloc (n.block_count * sizeof (*blocks));
  s = MULIB_MIR_NO_MEMORY;
  if (!r.code || !r.types || !map || !blocks) goto done;
  for (k = 0; k < g.program.node_count; ++k)
    {
      r.types[k] = g.nodes[k].type;
      if (g.nodes[k].opcode == MULIB_GSSAIR_PHI) r.types[g.program.node_count + phis++] = g.nodes[k].type;
    }
  r.types[nr] = r.types[nr + 1] = r.types[nr + 2] = MULIB_MIR_I64;
  for (k = 0; k < n.count; ++k)
    {
      const mulib_mir_native_instruction_t *a = &n.code[k];
      mulib_mir_flow_instruction_t i = mulib_clickir_flow_instruction_ (MULIB_MIR_FLOW_COPY);
      map[k] = pc; i.destination = a->destination; i.a = a->a; i.b = a->b;
      switch (a->opcode)
        {
        case MULIB_MIR_NATIVE_CONST: i.opcode = MULIB_MIR_FLOW_CONST;
          i.immediate.integer = a->immediate <= INT64_MAX ? (int64_t)a->immediate : -1 - (int64_t)(UINT64_MAX - a->immediate); break;
        case MULIB_MIR_NATIVE_PARAM: i.opcode = MULIB_MIR_FLOW_PARAM; break;
        case MULIB_MIR_NATIVE_COPY: break;
        case MULIB_MIR_NATIVE_ADD: i.opcode = MULIB_MIR_FLOW_WRAP_ADD; break;
        case MULIB_MIR_NATIVE_SUB: i.opcode = MULIB_MIR_FLOW_WRAP_SUB; break;
        case MULIB_MIR_NATIVE_MUL: i.opcode = MULIB_MIR_FLOW_WRAP_MUL; break;
        case MULIB_MIR_NATIVE_AND: i.opcode = MULIB_MIR_FLOW_AND; break;
        case MULIB_MIR_NATIVE_OR: i.opcode = MULIB_MIR_FLOW_OR; break;
        case MULIB_MIR_NATIVE_XOR: i.opcode = MULIB_MIR_FLOW_XOR; break;
        case MULIB_MIR_NATIVE_EQ: i.opcode = MULIB_MIR_FLOW_EQ; break;
        case MULIB_MIR_NATIVE_SLT: i.opcode = MULIB_MIR_FLOW_LT; break;
        case MULIB_MIR_NATIVE_SDIV: i.opcode = MULIB_MIR_FLOW_DIV; break;
        case MULIB_MIR_NATIVE_SREM: i.opcode = MULIB_MIR_FLOW_MOD; break;
        case MULIB_MIR_NATIVE_SHL: case MULIB_MIR_NATIVE_SHR: case MULIB_MIR_NATIVE_SAR:
          {
            mulib_mir_flow_instruction_t t = mulib_clickir_flow_instruction_ (MULIB_MIR_FLOW_CONST);
            t.destination = nr; t.immediate.integer = 63; r.code[pc++] = t;
            t = mulib_clickir_flow_instruction_ (MULIB_MIR_FLOW_AND);
            t.destination = nr + 1; t.a = a->b; t.b = nr; r.code[pc++] = t;
            i.b = nr + 1;
            i.opcode = a->opcode == MULIB_MIR_NATIVE_SHL ? MULIB_MIR_FLOW_SHL
                       : a->opcode == MULIB_MIR_NATIVE_SHR ? MULIB_MIR_FLOW_SHR : MULIB_MIR_FLOW_SAR;
          } break;
        case MULIB_MIR_NATIVE_ULT:
          {
            mulib_mir_flow_instruction_t t = mulib_clickir_flow_instruction_ (MULIB_MIR_FLOW_CONST);
            t.destination = nr; t.immediate.integer = INT64_MIN; r.code[pc++] = t;
            t = mulib_clickir_flow_instruction_ (MULIB_MIR_FLOW_XOR);
            t.destination = nr + 1; t.a = a->a; t.b = nr; r.code[pc++] = t;
            t.destination = nr + 2; t.a = a->b; r.code[pc++] = t;
            i.a = nr + 1; i.b = nr + 2; i.opcode = MULIB_MIR_FLOW_LT;
          } break;
        case MULIB_MIR_NATIVE_JUMP: i.opcode = MULIB_MIR_FLOW_JUMP; i.target = a->target; break;
        case MULIB_MIR_NATIVE_BRANCH:
          i.opcode = MULIB_MIR_FLOW_BRANCH; i.b = MULIB_MIR_NONE; i.target = a->target;
          r.code[pc++] = i;
          i = mulib_clickir_flow_instruction_ (MULIB_MIR_FLOW_JUMP); i.target = a->b; break;
        case MULIB_MIR_NATIVE_RETURN: i.opcode = MULIB_MIR_FLOW_RETURN; break;
        default: s = MULIB_MIR_UNSUPPORTED; goto done;
        }
      r.code[pc++] = i;
    }
  for (k = 0; k < n.block_count; ++k) blocks[k] = map[n.blocks[k].first];
  for (j = 0; j < pc; ++j)
    if (r.code[j].opcode == MULIB_MIR_FLOW_BRANCH || r.code[j].opcode == MULIB_MIR_FLOW_JUMP)
      r.code[j].target = blocks[r.code[j].target];
  r.program.code = r.code; r.program.count = pc; r.program.register_types = r.types;
  r.program.register_count = nr + 3; r.program.signature = g.program.signature;
  s = mulib_mir_flow_validate (&r.program, limit, d);
  if (s == MULIB_MIR_OK) { *out = r; memset (&r, 0, sizeof (r)); }
done:
  free (map); free (blocks); mulib_clickir_flow_free (&r);
  mulib_gssair_free (&g); mulib_mir_native_free (&n);
  return mulib_clickir_error_ (d, s, k);
}
mulib_mir_status_t
mulib_clickir_lower (const mulib_clickir_program_t *p, size_t limit,
                    mulib_mir_native_buffer_t *out, mulib_mir_diagnostic_t *d)
{
  mulib_gssair_buffer_t r;
  mulib_mir_status_t s;
  memset (&r, 0, sizeof (r));
  s = mulib_clickir_to_gssair (p, limit, &r, d);
  if (s == MULIB_MIR_OK) s = mulib_gssair_lower (&r.program, limit, out, d);
  mulib_gssair_free (&r); return s;
}
#ifdef __cplusplus
}
#endif
#endif
