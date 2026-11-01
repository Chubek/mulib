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
 * must still form a complete chain of LOAD/STORE/CALL operations per block. */
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
              if (n->memory != MULIB_MIR_NONE && !done[n->memory]) ready = 0;
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
  if (!out || out->edges || out->graph.nodes || out->program.graph.nodes)
    return mulib_clickir_error_ (d, MULIB_MIR_INVALID, 0);
  s = mulib_gssair_validate (p, limit, d);
  if (s == MULIB_MIR_OK) s = mulib_gssair_permute (p, NULL, limit, &r.graph, d);
  if (s == MULIB_MIR_OK) { r.program.graph = r.graph.program; *out = r; }
  return s;
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
