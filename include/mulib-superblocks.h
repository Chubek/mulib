/** @file mulib-superblocks.h
 * @brief Single-entry SSA traces with explicit side exits and native lowering.
 * Define MULIB_IMPL_SUPERBLOCKS, MULIB_IMPL_GSSAIR and MULIB_IMPL_MIR.
 */
#ifndef MULIB_SUPERBLOCKS_H
#define MULIB_SUPERBLOCKS_H
#include "mulib-gssair.h"
#ifdef __cplusplus
extern "C" {
#endif
/** order permutes all blocks; starts[trace_count+1] partitions it into nonempty
 * single-entry traces. Only trace heads can have outside predecessors. */
typedef struct mulib_superblocks_program {
  mulib_gssair_program_t graph;
  const size_t *order, *starts;
  size_t trace_count;
} mulib_superblocks_program_t;
typedef struct mulib_superblocks_buffer {
  mulib_gssair_buffer_t graph;
  size_t *order, *starts;
  mulib_superblocks_program_t program;
} mulib_superblocks_buffer_t;
/** Build deterministic maximal traces, preferring the true edge when both
 * continuations qualify. Copies graph structure and borrows signature metadata. */
mulib_mir_status_t mulib_superblocks_build (const mulib_gssair_program_t *program,
    size_t limit, mulib_superblocks_buffer_t *out, mulib_mir_diagnostic_t *diagnostic);
/** Check permutation, single-entry property, connectedness and SSA. */
mulib_mir_status_t mulib_superblocks_validate (const mulib_superblocks_program_t *program,
    size_t limit, mulib_mir_diagnostic_t *diagnostic);
/** Lay out SSA blocks in trace order, remapping nodes, branches and PHI gates. */
mulib_mir_status_t mulib_superblocks_to_gssair (const mulib_superblocks_program_t *program,
    size_t limit, mulib_gssair_buffer_t *out, mulib_mir_diagnostic_t *diagnostic);
/** Lower trace layout to native MIR without speculating across side exits. */
mulib_mir_status_t mulib_superblocks_lower (const mulib_superblocks_program_t *program,
    size_t limit, mulib_mir_native_buffer_t *out, mulib_mir_diagnostic_t *diagnostic);
/** Free owned arrays, not borrowed signature metadata. */
void mulib_superblocks_free (mulib_superblocks_buffer_t *buffer);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SUPERBLOCKS) && !defined(MULIB_SUPERBLOCKS_IMPLEMENTED)
#define MULIB_SUPERBLOCKS_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static mulib_mir_status_t
mulib_superblocks_error_ (mulib_mir_diagnostic_t *d, mulib_mir_status_t s, size_t k)
{ if (d) { d->status = s; d->record = k; } return s; }
void mulib_superblocks_free (mulib_superblocks_buffer_t *b)
{
  if (!b) return;
  mulib_gssair_free (&b->graph); free (b->order); free (b->starts); memset (b, 0, sizeof (*b));
}
static int
mulib_superblocks_continuation_ (const mulib_gssair_program_t *p, size_t from, size_t to)
{
  size_t k;
  if (!mulib_gssair_edge (p, from, to)) return 0;
  for (k = 0; k < p->block_count; ++k)
    if (k != from && mulib_gssair_edge (p, k, to)) return 0;
  return 1;
}
mulib_mir_status_t
mulib_superblocks_build (const mulib_gssair_program_t *p, size_t limit,
                        mulib_superblocks_buffer_t *out, mulib_mir_diagnostic_t *d)
{
  mulib_superblocks_buffer_t r;
  unsigned char *seen = NULL;
  size_t k = 0, at = 0, traces = 0;
  mulib_mir_status_t s;
  memset (&r, 0, sizeof (r));
  if (!out || out->order || out->starts || out->graph.nodes || out->program.order)
    return mulib_superblocks_error_ (d, MULIB_MIR_INVALID, 0);
  s = mulib_gssair_validate (p, limit, d);
  if (s != MULIB_MIR_OK) return s;
  if (p->block_count >= SIZE_MAX / sizeof (*r.starts))
    return mulib_superblocks_error_ (d, MULIB_MIR_RESOURCE_LIMIT, 0);
  s = mulib_gssair_permute (p, NULL, limit, &r.graph, d);
  if (s != MULIB_MIR_OK) return s;
  r.order = (size_t *)malloc (p->block_count * sizeof (*r.order));
  r.starts = (size_t *)malloc ((p->block_count + 1) * sizeof (*r.starts));
  seen = (unsigned char *)calloc (p->block_count, 1);
  s = MULIB_MIR_NO_MEMORY;
  if (!r.order || !r.starts || !seen) goto done;
  while (at < p->block_count)
    {
      for (k = 0; seen[k]; ++k) { }
      r.starts[traces++] = at;
      for (;;)
        {
          const mulib_gssair_block_t *b = &p->blocks[k];
          size_t next = MULIB_MIR_NONE;
          r.order[at++] = k; seen[k] = 1;
          if (b->yes != MULIB_MIR_NONE && !seen[b->yes]
              && mulib_superblocks_continuation_ (p, k, b->yes)) next = b->yes;
          else if (b->no != MULIB_MIR_NONE && !seen[b->no]
                   && mulib_superblocks_continuation_ (p, k, b->no)) next = b->no;
          if (next == MULIB_MIR_NONE) break;
          k = next;
        }
    }
  r.starts[traces] = at; r.program.graph = r.graph.program;
  r.program.order = r.order; r.program.starts = r.starts; r.program.trace_count = traces;
  *out = r; memset (&r, 0, sizeof (r)); s = MULIB_MIR_OK;
done:
  free (seen); mulib_superblocks_free (&r); return mulib_superblocks_error_ (d, s, k);
}
mulib_mir_status_t
mulib_superblocks_validate (const mulib_superblocks_program_t *p, size_t limit,
                           mulib_mir_diagnostic_t *d)
{
  unsigned char *seen;
  size_t k, t;
  mulib_mir_status_t s;
  if (!p || !p->order || !p->starts || !p->trace_count)
    return mulib_superblocks_error_ (d, MULIB_MIR_INVALID, 0);
  s = mulib_gssair_validate (&p->graph, limit, d);
  if (s != MULIB_MIR_OK) return s;
  if (p->trace_count > p->graph.block_count || p->starts[0] || p->order[0]
      || p->starts[p->trace_count] != p->graph.block_count)
    return mulib_superblocks_error_ (d, MULIB_MIR_BAD_REFERENCE, 0);
  seen = (unsigned char *)calloc (p->graph.block_count, 1);
  if (!seen) return mulib_superblocks_error_ (d, MULIB_MIR_NO_MEMORY, 0);
  s = MULIB_MIR_BAD_REFERENCE;
  for (k = 0; k < p->graph.block_count; ++k)
    { if (p->order[k] >= p->graph.block_count || seen[p->order[k]]) goto done; seen[p->order[k]] = 1; }
  for (t = 0; t < p->trace_count; ++t)
    {
      k = t;
      if (p->starts[t] >= p->starts[t + 1] || p->starts[t + 1] > p->graph.block_count) goto done;
      for (k = p->starts[t] + 1; k < p->starts[t + 1]; ++k)
        if (!mulib_superblocks_continuation_ (&p->graph, p->order[k - 1], p->order[k])) goto done;
    }
  s = MULIB_MIR_OK;
done:
  free (seen); return mulib_superblocks_error_ (d, s, k);
}
mulib_mir_status_t
mulib_superblocks_to_gssair (const mulib_superblocks_program_t *p, size_t limit,
                           mulib_gssair_buffer_t *out, mulib_mir_diagnostic_t *d)
{
  mulib_gssair_buffer_t r;
  mulib_gssair_block_t *blocks = NULL;
  size_t *order = NULL, *map = NULL, k, b, at = 0;
  mulib_mir_status_t s = mulib_superblocks_validate (p, limit, d);
  if (s != MULIB_MIR_OK) return s;
  if (!out || out->nodes || out->blocks || out->inputs || out->arguments || out->program.nodes)
    return mulib_superblocks_error_ (d, MULIB_MIR_INVALID, 0);
  memset (&r, 0, sizeof (r));
  order = (size_t *)malloc (p->graph.node_count * sizeof (*order));
  map = (size_t *)malloc (p->graph.block_count * sizeof (*map));
  blocks = (mulib_gssair_block_t *)malloc (p->graph.block_count * sizeof (*blocks));
  s = MULIB_MIR_NO_MEMORY;
  if (!order || !map || !blocks) goto done;
  for (b = 0; b < p->graph.block_count; ++b)
    {
      map[p->order[b]] = b;
      for (k = 0; k < p->graph.node_count; ++k)
        if (p->graph.nodes[k].block == p->order[b]) order[at++] = k;
    }
  s = mulib_gssair_permute (&p->graph, order, limit, &r, d);
  if (s != MULIB_MIR_OK) goto done;
  for (k = 0; k < p->graph.node_count; ++k) r.nodes[k].block = map[r.nodes[k].block];
  for (k = 0; k < p->graph.input_count; ++k) r.inputs[k].predecessor = map[r.inputs[k].predecessor];
  for (b = 0; b < p->graph.block_count; ++b)
    {
      blocks[b] = r.blocks[p->order[b]];
      if (blocks[b].yes != MULIB_MIR_NONE) blocks[b].yes = map[blocks[b].yes];
      if (blocks[b].no != MULIB_MIR_NONE) blocks[b].no = map[blocks[b].no];
    }
  memcpy (r.blocks, blocks, p->graph.block_count * sizeof (*blocks));
  s = mulib_gssair_validate (&r.program, limit, d);
  if (s == MULIB_MIR_OK) { *out = r; memset (&r, 0, sizeof (r)); }
done:
  free (order); free (map); free (blocks); mulib_gssair_free (&r);
  if (s == MULIB_MIR_NO_MEMORY) return mulib_superblocks_error_ (d, s, 0);
  return s;
}
mulib_mir_status_t
mulib_superblocks_lower (const mulib_superblocks_program_t *p, size_t limit,
                        mulib_mir_native_buffer_t *out, mulib_mir_diagnostic_t *d)
{
  mulib_gssair_buffer_t r;
  mulib_mir_status_t s;
  memset (&r, 0, sizeof (r));
  s = mulib_superblocks_to_gssair (p, limit, &r, d);
  if (s == MULIB_MIR_OK) s = mulib_gssair_lower (&r.program, limit, out, d);
  mulib_gssair_free (&r); return s;
}
#ifdef __cplusplus
}
#endif
#endif
