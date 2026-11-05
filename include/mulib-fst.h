/**
 * @file mulib-fst.h
 * @brief Deterministic range finite-state transducers over uint32_t symbols.
 * Define MULIB_IMPL_FST and MULIB_IMPL_VEC in one translation unit.
 * Each transition consumes one symbol and optionally emits one constant symbol.
 * No epsilon transitions, overlapping input ranges or implicit fallback edges.
 */
#ifndef MULIB_FST_H
#define MULIB_FST_H
#include "mulib-vec.h"
#include "mulib-buffer.h"
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_FST_NONE SIZE_MAX
/** NOMATCH is not an API error. Zero budgets select documented defaults. */
typedef enum mulib_fst_status {
  MULIB_FST_OK = 0, MULIB_FST_NOMATCH = 1,
  MULIB_FST_INVALID = -1, MULIB_FST_NOMEM = -2,
  MULIB_FST_LIMIT = -3, MULIB_FST_CONFLICT = -4
} mulib_fst_status_t;
/** Owned graph; stable IDs, no deletion, no mutable borrowed records. */
typedef struct mulib_fst mulib_fst_t;
/** Copied state metadata; first_edge and edge.next form an outgoing list. */
typedef struct mulib_fst_state { int accepting; size_t first_edge; } mulib_fst_state_t;
/** Inclusive input range and optional constant output. */
typedef struct mulib_fst_edge {
  size_t from, to, next;
  uint32_t lower, upper, output;
  int emits;
} mulib_fst_edge_t;
/** Owned successful result. Initialize to {0}; use mulib_fst_result_free(). */
typedef struct mulib_fst_result {
  uint32_t *output;
  size_t length, final_state;
} mulib_fst_result_t;
/** Create an empty graph; defaults: 65536 states, 262144 transitions. */
mulib_fst_t *mulib_fst_create (size_t max_states, size_t max_edges);
/** Release graph; NULL allowed. */
void mulib_fst_destroy (mulib_fst_t *fst);
/** Append state; required out_id and graph are unchanged on failure. */
int mulib_fst_add_state (mulib_fst_t *fst, int accepting, size_t *out_id);
/** Update an existing state's accepting flag. */
int mulib_fst_set_accepting (mulib_fst_t *fst, size_t state, int accepting);
/** Add edge; overlapping ranges from one state return CONFLICT, even if the
 * destinations/outputs agree. out_id is optional; failure is transactional. */
int mulib_fst_add_range (mulib_fst_t *fst, size_t from, size_t to,
                         uint32_t lower, uint32_t upper, int emits,
                         uint32_t output, size_t *out_id);
/** Copy state/edge; missing IDs or NULL outputs return INVALID. */
int mulib_fst_state (const mulib_fst_t *fst, size_t id, mulib_fst_state_t *out);
/** Copy a transition. */
int mulib_fst_edge (const mulib_fst_t *fst, size_t id, mulib_fst_edge_t *out);
/** Counts, zero for NULL. */
size_t mulib_fst_state_count (const mulib_fst_t *fst);
/** Transition count, zero for NULL. */
size_t mulib_fst_edge_count (const mulib_fst_t *fst);
/** One step. Required next/emits/output change only on success. work is optional
 * and set on every return. Budget counts one token plus each inspected edge;
 * default 10000000. A missing edge returns NOMATCH without changing state. */
int mulib_fst_step (const mulib_fst_t *fst, size_t state, uint32_t token,
                    size_t max_steps, size_t *next, int *emits,
                    uint32_t *output, size_t *work);
/** Transduce entire input into an accepting state. NULL tokens only for length
 * zero. out must be zero-initialized or a previous result; success replaces it,
 * failure (including NOMATCH) preserves it. One shared budget covers the run. */
int mulib_fst_transduce (const mulib_fst_t *fst, size_t start,
                         const uint32_t *tokens, size_t length, size_t max_steps,
                         mulib_fst_result_t *out, size_t *work);
/** Release result and zero it; NULL allowed. */
void mulib_fst_result_free (mulib_fst_result_t *result);
/** Portable LE/CRC graph snapshots. Start state is caller metadata.
 * Atomic append and owned decode; out is unchanged on error. Additionally
 * define MULIB_IMPL_FST_SERIALIZE and BUFFER, ENDIAN and CRC32 implementations
 * before first inclusion for snapshot functions. Core FST dependencies unchanged. */
int mulib_fst_serialize(const mulib_fst_t *, mulib_buffer_t *);
int mulib_fst_deserialize(const void *, size_t, size_t max_states, size_t max_edges,
    mulib_fst_t **out);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_FST) && !defined(MULIB_FST_IMPLEMENTED)
#define MULIB_FST_IMPLEMENTED
#ifdef MULIB_IMPL_FST_SERIALIZE
#include "mulib-endian.h"
#include "mulib-crc32.h"
#endif
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
struct mulib_fst {
  mulib_vec_t states, edges;
  size_t max_states, max_edges;
};
static const mulib_fst_state_t *
mulib_fst_state_ (const mulib_fst_t *f, size_t i)
{ return (const mulib_fst_state_t *)(f->states.data + i * sizeof (mulib_fst_state_t)); }
static const mulib_fst_edge_t *
mulib_fst_edge_ (const mulib_fst_t *f, size_t i)
{ return (const mulib_fst_edge_t *)(f->edges.data + i * sizeof (mulib_fst_edge_t)); }
mulib_fst_t *
mulib_fst_create (size_t states, size_t edges)
{
  mulib_fst_t *f = (mulib_fst_t *)malloc (sizeof (*f));
  if (!f) return NULL;
  mulib_vec_init (&f->states, sizeof (mulib_fst_state_t));
  mulib_vec_init (&f->edges, sizeof (mulib_fst_edge_t));
  f->max_states = states ? states : 65536;
  f->max_edges = edges ? edges : 262144;
  return f;
}
void mulib_fst_destroy (mulib_fst_t *f)
{
  if (!f) return;
  mulib_vec_free (&f->states); mulib_vec_free (&f->edges); free (f);
}
int mulib_fst_add_state (mulib_fst_t *f, int accepting, size_t *out)
{
  mulib_fst_state_t s;
  if (!f || !out) return MULIB_FST_INVALID;
  if (f->states.length >= f->max_states
      || f->states.length >= SIZE_MAX / sizeof (s)) return MULIB_FST_LIMIT;
  s.accepting = accepting != 0; s.first_edge = MULIB_FST_NONE;
  if (!mulib_vec_push (&f->states, &s)) return MULIB_FST_NOMEM;
  *out = f->states.length - 1; return MULIB_FST_OK;
}
int mulib_fst_set_accepting (mulib_fst_t *f, size_t id, int accepting)
{
  mulib_fst_state_t *s;
  if (!f || id >= f->states.length) return MULIB_FST_INVALID;
  s = (mulib_fst_state_t *)mulib_vec_at (&f->states, id);
  s->accepting = accepting != 0; return MULIB_FST_OK;
}
int mulib_fst_add_range (mulib_fst_t *f, size_t from, size_t to,
                         uint32_t lower, uint32_t upper, int emits,
                         uint32_t output, size_t *out)
{
  mulib_fst_state_t *s;
  mulib_fst_edge_t e;
  size_t k;
  if (!f || from >= f->states.length || to >= f->states.length || lower > upper)
    return MULIB_FST_INVALID;
  s = (mulib_fst_state_t *)mulib_vec_at (&f->states, from);
  for (k = s->first_edge; k != MULIB_FST_NONE; k = mulib_fst_edge_ (f, k)->next)
    {
      const mulib_fst_edge_t *old = mulib_fst_edge_ (f, k);
      if (lower <= old->upper && old->lower <= upper) return MULIB_FST_CONFLICT;
    }
  if (f->edges.length >= f->max_edges
      || f->edges.length >= SIZE_MAX / sizeof (e)) return MULIB_FST_LIMIT;
  e.from = from; e.to = to; e.next = s->first_edge;
  e.lower = lower; e.upper = upper; e.output = output; e.emits = emits != 0;
  if (!mulib_vec_push (&f->edges, &e)) return MULIB_FST_NOMEM;
  s->first_edge = f->edges.length - 1;
  if (out) *out = s->first_edge;
  return MULIB_FST_OK;
}
int mulib_fst_state (const mulib_fst_t *f, size_t id, mulib_fst_state_t *out)
{
  if (!f || !out || id >= f->states.length) return MULIB_FST_INVALID;
  *out = *mulib_fst_state_ (f, id); return MULIB_FST_OK;
}
int mulib_fst_edge (const mulib_fst_t *f, size_t id, mulib_fst_edge_t *out)
{
  if (!f || !out || id >= f->edges.length) return MULIB_FST_INVALID;
  *out = *mulib_fst_edge_ (f, id); return MULIB_FST_OK;
}
size_t mulib_fst_state_count (const mulib_fst_t *f) { return f ? f->states.length : 0; }
size_t mulib_fst_edge_count (const mulib_fst_t *f) { return f ? f->edges.length : 0; }
static int mulib_fst_tick_ (size_t *work, size_t limit)
{ if (*work >= limit) return 0; ++*work; return 1; }
static int mulib_fst_move_ (const mulib_fst_t *f, size_t state, uint32_t token,
                           size_t limit, size_t *work, mulib_fst_edge_t *edge)
{
  size_t k;
  if (!mulib_fst_tick_ (work, limit)) return MULIB_FST_LIMIT;
  for (k = mulib_fst_state_ (f, state)->first_edge; k != MULIB_FST_NONE;
       k = mulib_fst_edge_ (f, k)->next)
    {
      const mulib_fst_edge_t *e = mulib_fst_edge_ (f, k);
      if (!mulib_fst_tick_ (work, limit)) return MULIB_FST_LIMIT;
      if (token >= e->lower && token <= e->upper) { *edge = *e; return MULIB_FST_OK; }
    }
  return MULIB_FST_NOMATCH;
}
int mulib_fst_step (const mulib_fst_t *f, size_t state, uint32_t token,
                    size_t limit, size_t *next, int *emits, uint32_t *output, size_t *work)
{
  size_t spent = 0;
  mulib_fst_edge_t e;
  int status = MULIB_FST_INVALID;
  if (f && state < f->states.length && next && emits && output)
    {
      status = mulib_fst_move_ (f, state, token, limit ? limit : 10000000, &spent, &e);
      if (status == MULIB_FST_OK) { *next = e.to; *emits = e.emits; *output = e.output; }
    }
  if (work) *work = spent;
  return status;
}
int mulib_fst_transduce (const mulib_fst_t *f, size_t state, const uint32_t *tokens,
                         size_t length, size_t limit, mulib_fst_result_t *out, size_t *work)
{
  mulib_vec_t values;
  size_t i, spent = 0;
  int status = MULIB_FST_INVALID;
  mulib_vec_init (&values, sizeof (uint32_t));
  if (!f || state >= f->states.length || (!tokens && length) || !out) goto done;
  if (length > SIZE_MAX / sizeof (*tokens)) { status = MULIB_FST_LIMIT; goto done; }
  if (!limit) limit = 10000000;
  for (i = 0; i < length; ++i)
    {
      mulib_fst_edge_t e;
      status = mulib_fst_move_ (f, state, tokens[i], limit, &spent, &e);
      if (status != MULIB_FST_OK) goto done;
      if (e.emits && !mulib_vec_push (&values, &e.output))
        { status = MULIB_FST_NOMEM; goto done; }
      state = e.to;
    }
  status = MULIB_FST_NOMATCH;
  if (!mulib_fst_state_ (f, state)->accepting) goto done;
  mulib_fst_result_free (out);
  out->output = (uint32_t *)values.data; out->length = values.length; out->final_state = state;
  values.data = NULL; status = MULIB_FST_OK;
done:
  mulib_vec_free (&values);
  if (work) *work = spent;
  return status;
}
void mulib_fst_result_free (mulib_fst_result_t *r)
{ if (r) { free (r->output); memset (r, 0, sizeof (*r)); } }
#ifdef MULIB_IMPL_FST_SERIALIZE
int mulib_fst_serialize(const mulib_fst_t *f, mulib_buffer_t *out) {
  mulib_buffer_t b = {0};
  unsigned char header[16], record[24], crc[4];
  size_t i;
  int rc = MULIB_FST_NOMEM;
  if (!f || !out || f->states.length > UINT32_MAX || f->edges.length > UINT32_MAX) return MULIB_FST_INVALID;
  memcpy(header, "MuFST\1\0\0", 8);
  mulib_endian_store_le32(header + 8, (uint32_t)f->states.length);
  mulib_endian_store_le32(header + 12, (uint32_t)f->edges.length);
  if (!mulib_buffer_append(&b, header, 16)) goto done;
  for (i = 0; i < f->states.length; ++i) {
    unsigned char a = (unsigned char)mulib_fst_state_(f, i)->accepting;
    if (!mulib_buffer_append(&b, &a, 1)) goto done;
  }
  for (i = 0; i < f->edges.length; ++i) {
    const mulib_fst_edge_t *e = mulib_fst_edge_(f, i);
    mulib_endian_store_le32(record, (uint32_t)e->from); mulib_endian_store_le32(record + 4, (uint32_t)e->to);
    mulib_endian_store_le32(record + 8, e->lower); mulib_endian_store_le32(record + 12, e->upper);
    mulib_endian_store_le32(record + 16, e->output); mulib_endian_store_le32(record + 20, (uint32_t)e->emits);
    if (!mulib_buffer_append(&b, record, 24)) goto done;
  }
  mulib_endian_store_le32(crc, mulib_crc32(b.data, b.length));
  if (!mulib_buffer_append(&b, crc, 4) || !mulib_buffer_append(out, b.data, b.length)) goto done;
  rc = MULIB_FST_OK;
done:
  mulib_buffer_free(&b); return rc;
}
int mulib_fst_deserialize(const void *data, size_t n, size_t ms, size_t me, mulib_fst_t **out) {
  const unsigned char *p = (const unsigned char *)data;
  mulib_fst_t *f;
  uint32_t *degree;
  size_t ns, ne, i, id, pos;
  size_t inspections = 0;
  int rc;
  if (!out || (!p && n) || n < 20 || memcmp(p, "MuFST\1\0\0", 8)) return MULIB_FST_INVALID;
  ns = mulib_endian_load_le32(p + 8); ne = mulib_endian_load_le32(p + 12);
  if (ns > (ms ? ms : 65536) || ne > (me ? me : 262144)) return MULIB_FST_LIMIT;
  if (ns > n - 20 || ne > (n - 20 - ns) / 24 || 20 + ns + ne * 24 != n ||
      mulib_endian_load_le32(p + n - 4) != mulib_crc32(p, n - 4)) return MULIB_FST_INVALID;
  f = mulib_fst_create(ms, me); if (!f) return MULIB_FST_NOMEM;
  degree = (uint32_t *)calloc(ns ? ns : 1, sizeof *degree);
  if (!degree) { mulib_fst_destroy(f); return MULIB_FST_NOMEM; }
  for (i = 0; i < ns; ++i) {
    if (p[16 + i] > 1) { rc = MULIB_FST_INVALID; goto fail; }
    rc = mulib_fst_add_state(f, p[16 + i], &id); if (rc) goto fail;
  }
  pos = 16 + ns;
  for (i = 0; i < ne; ++i, pos += 24) {
    uint32_t emits = mulib_endian_load_le32(p + pos + 20);
    uint32_t from = mulib_endian_load_le32(p + pos);
    if (emits > 1) { rc = MULIB_FST_INVALID; goto fail; }
    if (from >= ns) { rc = MULIB_FST_INVALID; goto fail; }
    if (degree[from] > 10000000u - inspections) { rc = MULIB_FST_LIMIT; goto fail; }
    inspections += degree[from]++;
    rc = mulib_fst_add_range(f, mulib_endian_load_le32(p + pos), mulib_endian_load_le32(p + pos + 4),
        mulib_endian_load_le32(p + pos + 8), mulib_endian_load_le32(p + pos + 12),
        (int)emits, mulib_endian_load_le32(p + pos + 16), NULL);
    if (rc) goto fail;
  }
  free(degree); *out = f; return MULIB_FST_OK;
fail:
  free(degree); mulib_fst_destroy(f); return rc;
}
#endif
#ifdef __cplusplus
}
#endif
#endif
