/** @file mulib-depir.h
 * @brief Typed entity/dependence graph deriving executable compiler IRs.
 * Define MULIB_IMPL_DEPIR and the documented transitive implementation switches.
 */
#ifndef MULIB_DEPIR_H
#define MULIB_DEPIR_H
#include "mulib-clickir.h"
#include "mulib-anfir.h"
#include "mulib-superblocks.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_depir_kind {
  MULIB_DEPIR_VALUE, MULIB_DEPIR_BLOCK, MULIB_DEPIR_FUNCTION,
  MULIB_DEPIR_TYPE, MULIB_DEPIR_DATA
} mulib_depir_kind_t;
/** index names a graph value, block, external function, MIR type, or CONST
 * node (DATA). Entities can have cyclic metadata relationships. */
typedef struct mulib_depir_entity { mulib_depir_kind_t kind; size_t index; }
  mulib_depir_entity_t;
typedef enum mulib_depir_relation {
  MULIB_DEPIR_DATA_FLOW, MULIB_DEPIR_CONTROL, MULIB_DEPIR_MEMORY,
  MULIB_DEPIR_TYPING, MULIB_DEPIR_CALL
} mulib_depir_relation_t;
typedef struct mulib_depir_edge {
  size_t before, after;
  mulib_depir_relation_t relation;
} mulib_depir_edge_t;
/** Borrowed graph and metadata. Executable operands/CFG live in graph; extra
 * VALUE->VALUE data/control/memory edges constrain local node order. Other
 * relations describe metadata, CFG and inter-block dependence, not execution
 * of additional operations. Derivation never topologically sorts CFG cycles. */
typedef struct mulib_depir_program {
  mulib_gssair_program_t graph;
  const mulib_depir_entity_t *entities;
  size_t entity_count;
  const mulib_depir_edge_t *edges;
  size_t edge_count;
} mulib_depir_program_t;
typedef struct mulib_depir_buffer {
  mulib_gssair_buffer_t graph;
  mulib_depir_entity_t *entities;
  mulib_depir_edge_t *edges;
  mulib_depir_program_t program;
} mulib_depir_buffer_t;
/** Derive complete value, block, function, type, constant-data and dependency
 * records from SSA. Copies structure; signature/function metadata is borrowed. */
mulib_mir_status_t mulib_depir_from_gssair (const mulib_gssair_program_t *program,
    size_t limit, mulib_depir_buffer_t *out, mulib_mir_diagnostic_t *diagnostic);
/** Validate graph shape, entity indices and relation endpoints. */
mulib_mir_status_t mulib_depir_validate (const mulib_depir_program_t *program,
    size_t limit, mulib_mir_diagnostic_t *diagnostic);
/** Derive a Sea of Nodes retaining executable local ordering constraints. */
mulib_mir_status_t mulib_depir_to_clickir (const mulib_depir_program_t *program,
    size_t limit, mulib_clickir_buffer_t *out, mulib_mir_diagnostic_t *diagnostic);
/** Derive ordered, dominance-checked SSA. */
mulib_mir_status_t mulib_depir_to_gssair (const mulib_depir_program_t *program,
    size_t limit, mulib_gssair_buffer_t *out, mulib_mir_diagnostic_t *diagnostic);
/** Derive A-normal bindings from the ordered execution graph. */
mulib_mir_status_t mulib_depir_to_anfir (const mulib_depir_program_t *program,
    size_t limit, mulib_anfir_buffer_t *out, mulib_mir_diagnostic_t *diagnostic);
/** Derive single-entry superblocks from the ordered execution graph. */
mulib_mir_status_t mulib_depir_to_superblocks (const mulib_depir_program_t *program,
    size_t limit, mulib_superblocks_buffer_t *out, mulib_mir_diagnostic_t *diagnostic);
/** Derive SSA and lower it to native MIR. */
mulib_mir_status_t mulib_depir_lower (const mulib_depir_program_t *program,
    size_t limit, mulib_mir_native_buffer_t *out, mulib_mir_diagnostic_t *diagnostic);
/** Release owned arrays, not borrowed signature metadata. */
void mulib_depir_free (mulib_depir_buffer_t *buffer);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DEPIR) && !defined(MULIB_DEPIR_IMPLEMENTED)
#define MULIB_DEPIR_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static mulib_mir_status_t
mulib_depir_error_ (mulib_mir_diagnostic_t *d, mulib_mir_status_t s, size_t k)
{ if (d) { d->status = s; d->record = k; } return s; }
void mulib_depir_free (mulib_depir_buffer_t *b)
{
  if (!b) return;
  mulib_gssair_free (&b->graph); free (b->entities); free (b->edges); memset (b, 0, sizeof (*b));
}
mulib_mir_status_t
mulib_depir_validate (const mulib_depir_program_t *p, size_t limit, mulib_mir_diagnostic_t *d)
{
  size_t k;
  mulib_mir_status_t s;
  if (!p || (p->entity_count && !p->entities) || (p->edge_count && !p->edges))
    return mulib_depir_error_ (d, MULIB_MIR_INVALID, 0);
  s = mulib_gssair_shape (&p->graph, limit, d);
  if (s != MULIB_MIR_OK) return s;
  if (p->entity_count > limit || p->edge_count > limit
      || p->entity_count > SIZE_MAX / sizeof (*p->entities)
      || p->edge_count > SIZE_MAX / sizeof (*p->edges))
    return mulib_depir_error_ (d, MULIB_MIR_RESOURCE_LIMIT, 0);
  for (k = 0; k < p->entity_count; ++k)
    {
      const mulib_depir_entity_t *e = &p->entities[k];
      int valid = 0;
      switch (e->kind)
        {
        case MULIB_DEPIR_VALUE: valid = e->index < p->graph.node_count; break;
        case MULIB_DEPIR_BLOCK: valid = e->index < p->graph.block_count; break;
        case MULIB_DEPIR_FUNCTION: valid = e->index < p->graph.function_count; break;
        case MULIB_DEPIR_TYPE: valid = e->index == MULIB_MIR_I64 || e->index == MULIB_MIR_BOOL || e->index == MULIB_MIR_WORD; break;
        case MULIB_DEPIR_DATA: valid = e->index < p->graph.node_count && p->graph.nodes[e->index].opcode == MULIB_MIR_NATIVE_CONST; break;
        default: break;
        }
      if (!valid) return mulib_depir_error_ (d, MULIB_MIR_BAD_REFERENCE, k);
    }
  for (k = 0; k < p->edge_count; ++k)
    if (p->edges[k].before >= p->entity_count || p->edges[k].after >= p->entity_count
        || p->edges[k].relation < MULIB_DEPIR_DATA_FLOW || p->edges[k].relation > MULIB_DEPIR_CALL)
      return mulib_depir_error_ (d, MULIB_MIR_BAD_REFERENCE, k);
  return mulib_depir_error_ (d, MULIB_MIR_OK, 0);
}
static size_t mulib_depir_type_ (mulib_mir_type_t t)
{ return t == MULIB_MIR_I64 ? 0 : t == MULIB_MIR_BOOL ? 1 : 2; }
static int
mulib_depir_put_ (mulib_depir_edge_t *edges, size_t *count, size_t limit,
                 size_t before, size_t after, mulib_depir_relation_t relation)
{
  if (*count == limit) return 0;
  if (edges) { edges[*count].before = before; edges[*count].after = after; edges[*count].relation = relation; }
  ++*count; return 1;
}
mulib_mir_status_t
mulib_depir_from_gssair (const mulib_gssair_program_t *p, size_t limit,
                       mulib_depir_buffer_t *out, mulib_mir_diagnostic_t *d)
{
  mulib_depir_buffer_t r;
  size_t k = 0, j, count, entities, blocks, funcs, types, data, dc;
  int pass;
  mulib_mir_status_t s;
  memset (&r, 0, sizeof (r));
  if (!out || out->entities || out->edges || out->graph.nodes || out->program.entities)
    return mulib_depir_error_ (d, MULIB_MIR_INVALID, 0);
  s = mulib_gssair_validate (p, limit, d);
  if (s != MULIB_MIR_OK) return s;
  s = MULIB_MIR_RESOURCE_LIMIT;
  blocks = p->node_count;
  if (p->block_count > limit - blocks) goto done;
  funcs = blocks + p->block_count;
  if (p->function_count > limit - funcs) goto done;
  types = funcs + p->function_count;
  if (limit - types < 3) goto done;
  data = types + 3; entities = data;
  for (k = 0; k < p->node_count; ++k)
    if (p->nodes[k].opcode == MULIB_MIR_NATIVE_CONST)
      { if (entities == limit) goto done; ++entities; }
  if (entities > SIZE_MAX / sizeof (*r.entities)) goto done;
  r.entities = (mulib_depir_entity_t *)malloc (entities * sizeof (*r.entities));
  s = MULIB_MIR_NO_MEMORY;
  if (!r.entities) goto done;
  dc = data;
  for (k = 0; k < p->node_count; ++k)
    {
      r.entities[k].kind = MULIB_DEPIR_VALUE; r.entities[k].index = k;
      if (p->nodes[k].opcode == MULIB_MIR_NATIVE_CONST)
        { r.entities[dc].kind = MULIB_DEPIR_DATA; r.entities[dc++].index = k; }
    }
  for (k = 0; k < p->block_count; ++k)
    { r.entities[blocks + k].kind = MULIB_DEPIR_BLOCK; r.entities[blocks + k].index = k; }
  for (k = 0; k < p->function_count; ++k)
    { r.entities[funcs + k].kind = MULIB_DEPIR_FUNCTION; r.entities[funcs + k].index = k; }
  for (k = 0; k < 3; ++k)
    { r.entities[types + k].kind = MULIB_DEPIR_TYPE; r.entities[types + k].index = k == 0 ? MULIB_MIR_I64 : k == 1 ? MULIB_MIR_BOOL : MULIB_MIR_WORD; }
  for (pass = 0; pass < 2; ++pass)
    {
      count = 0; dc = data; s = MULIB_MIR_RESOURCE_LIMIT;
#define MULIB_DEPIR_EDGE_(a,b,rn) do { if (!mulib_depir_put_ (r.edges, &count, limit, (a), (b), (rn))) goto done; } while (0)
      for (k = 0; k < p->node_count; ++k)
        {
          const mulib_gssair_node_t *n = &p->nodes[k];
          int arity = mulib_mir_native_arity (n->opcode);
          MULIB_DEPIR_EDGE_ (blocks + n->block, k, MULIB_DEPIR_CONTROL);
          if (n->type != MULIB_MIR_VOID) MULIB_DEPIR_EDGE_ (types + mulib_depir_type_ (n->type), k, MULIB_DEPIR_TYPING);
          if (arity > 0) MULIB_DEPIR_EDGE_ (n->a, k, MULIB_DEPIR_DATA_FLOW);
          if (arity == 2) MULIB_DEPIR_EDGE_ (n->b, k, MULIB_DEPIR_DATA_FLOW);
          if (n->memory != MULIB_MIR_NONE) MULIB_DEPIR_EDGE_ (n->memory, k, MULIB_DEPIR_MEMORY);
          if (n->opcode == MULIB_MIR_NATIVE_CONST) { MULIB_DEPIR_EDGE_ (dc, k, MULIB_DEPIR_DATA_FLOW); ++dc; }
          if (n->opcode == MULIB_GSSAIR_PHI)
            for (j = 0; j < n->b; ++j)
              {
                MULIB_DEPIR_EDGE_ (p->inputs[n->a + j].value, k, MULIB_DEPIR_DATA_FLOW);
                MULIB_DEPIR_EDGE_ (blocks + p->inputs[n->a + j].predecessor, k, MULIB_DEPIR_CONTROL);
              }
          if (n->opcode == MULIB_MIR_NATIVE_CALL)
            {
              MULIB_DEPIR_EDGE_ (funcs + (size_t)n->immediate, k, MULIB_DEPIR_CALL);
              for (j = 0; j < n->b; ++j) MULIB_DEPIR_EDGE_ (p->arguments[n->a + j], k, MULIB_DEPIR_DATA_FLOW);
            }
        }
      for (k = 0; k < p->block_count; ++k)
        {
          const mulib_gssair_block_t *b = &p->blocks[k];
          if (b->value != MULIB_MIR_NONE) MULIB_DEPIR_EDGE_ (b->value, blocks + k, MULIB_DEPIR_CONTROL);
          if (b->yes != MULIB_MIR_NONE) MULIB_DEPIR_EDGE_ (blocks + k, blocks + b->yes, MULIB_DEPIR_CONTROL);
          if (b->no != MULIB_MIR_NONE) MULIB_DEPIR_EDGE_ (blocks + k, blocks + b->no, MULIB_DEPIR_CONTROL);
        }
      for (k = 0; k < p->function_count; ++k)
        {
          const mulib_mir_signature_t *f = &p->functions[k].signature;
          MULIB_DEPIR_EDGE_ (types + mulib_depir_type_ (f->result), funcs + k, MULIB_DEPIR_TYPING);
          for (j = 0; j < f->parameter_count; ++j)
            MULIB_DEPIR_EDGE_ (types + mulib_depir_type_ (f->parameters[j]), funcs + k, MULIB_DEPIR_TYPING);
        }
#undef MULIB_DEPIR_EDGE_
      if (!pass)
        {
          if (count > SIZE_MAX / sizeof (*r.edges)) goto done;
          r.edges = (mulib_depir_edge_t *)malloc (count * sizeof (*r.edges));
          s = MULIB_MIR_NO_MEMORY;
          if (!r.edges) goto done;
        }
    }
  s = mulib_gssair_permute (p, NULL, limit, &r.graph, d);
  if (s != MULIB_MIR_OK) goto done;
  r.program.graph = r.graph.program; r.program.entities = r.entities;
  r.program.entity_count = entities; r.program.edges = r.edges; r.program.edge_count = count;
  *out = r; memset (&r, 0, sizeof (r));
done:
  mulib_depir_free (&r); return mulib_depir_error_ (d, s, k);
}
mulib_mir_status_t
mulib_depir_to_clickir (const mulib_depir_program_t *p, size_t limit,
                      mulib_clickir_buffer_t *out, mulib_mir_diagnostic_t *d)
{
  mulib_clickir_buffer_t r;
  size_t k, count = 0;
  mulib_mir_status_t s = mulib_depir_validate (p, limit, d);
  if (s != MULIB_MIR_OK) return s;
  if (!out || out->edges || out->graph.nodes || out->program.graph.nodes)
    return mulib_depir_error_ (d, MULIB_MIR_INVALID, 0);
  memset (&r, 0, sizeof (r));
  s = mulib_gssair_permute (&p->graph, NULL, limit, &r.graph, d);
  if (s != MULIB_MIR_OK) return s;
  if (p->edge_count) r.edges = (mulib_clickir_edge_t *)malloc (p->edge_count * sizeof (*r.edges));
  if (p->edge_count && !r.edges)
    { mulib_clickir_free (&r); return mulib_depir_error_ (d, MULIB_MIR_NO_MEMORY, 0); }
  for (k = 0; k < p->edge_count; ++k)
    {
      const mulib_depir_edge_t *e = &p->edges[k];
      const mulib_depir_entity_t *a = &p->entities[e->before], *b = &p->entities[e->after];
      if (a->kind == MULIB_DEPIR_VALUE && b->kind == MULIB_DEPIR_VALUE
          && e->relation <= MULIB_DEPIR_MEMORY
          && p->graph.nodes[a->index].block == p->graph.nodes[b->index].block
          && (p->graph.nodes[b->index].opcode != MULIB_GSSAIR_PHI
              || e->relation != MULIB_DEPIR_DATA_FLOW))
        { r.edges[count].before = a->index; r.edges[count++].after = b->index; }
    }
  r.program.graph = r.graph.program; r.program.edges = r.edges; r.program.edge_count = count;
  *out = r; return MULIB_MIR_OK;
}
mulib_mir_status_t
mulib_depir_to_gssair (const mulib_depir_program_t *p, size_t limit,
                     mulib_gssair_buffer_t *out, mulib_mir_diagnostic_t *d)
{
  mulib_clickir_buffer_t r;
  mulib_mir_status_t s;
  memset (&r, 0, sizeof (r));
  s = mulib_depir_to_clickir (p, limit, &r, d);
  if (s == MULIB_MIR_OK) s = mulib_clickir_to_gssair (&r.program, limit, out, d);
  mulib_clickir_free (&r); return s;
}
mulib_mir_status_t
mulib_depir_to_anfir (const mulib_depir_program_t *p, size_t limit,
                    mulib_anfir_buffer_t *out, mulib_mir_diagnostic_t *d)
{
  mulib_gssair_buffer_t r;
  mulib_mir_status_t s;
  memset (&r, 0, sizeof (r));
  s = mulib_depir_to_gssair (p, limit, &r, d);
  if (s == MULIB_MIR_OK) s = mulib_anfir_from_gssair (&r.program, limit, out, d);
  mulib_gssair_free (&r); return s;
}
mulib_mir_status_t
mulib_depir_to_superblocks (const mulib_depir_program_t *p, size_t limit,
                          mulib_superblocks_buffer_t *out, mulib_mir_diagnostic_t *d)
{
  mulib_gssair_buffer_t r;
  mulib_mir_status_t s;
  memset (&r, 0, sizeof (r));
  s = mulib_depir_to_gssair (p, limit, &r, d);
  if (s == MULIB_MIR_OK) s = mulib_superblocks_build (&r.program, limit, out, d);
  mulib_gssair_free (&r); return s;
}
mulib_mir_status_t
mulib_depir_lower (const mulib_depir_program_t *p, size_t limit,
                  mulib_mir_native_buffer_t *out, mulib_mir_diagnostic_t *d)
{
  mulib_gssair_buffer_t r;
  mulib_mir_status_t s;
  memset (&r, 0, sizeof (r));
  s = mulib_depir_to_gssair (p, limit, &r, d);
  if (s == MULIB_MIR_OK) s = mulib_gssair_lower (&r.program, limit, out, d);
  mulib_gssair_free (&r); return s;
}
#ifdef __cplusplus
}
#endif
#endif
