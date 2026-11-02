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
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_FST) && !defined(MULIB_FST_IMPLEMENTED)
#define MULIB_FST_IMPLEMENTED
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
#ifdef __cplusplus
}
#endif
#endif
