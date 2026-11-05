/** @file mulib-dfa.h
 * @brief Range DFAs and bounded epsilon-NFA subset construction over uint32 symbols.
 * Define MULIB_IMPL_DFA and FST, FST_SERIALIZE, VEC, BUFFER, ENDIAN, CRC32.
 */
#ifndef MULIB_DFA_H
#define MULIB_DFA_H
#include "mulib-fst.h"
#include "mulib-endian.h"
#include "mulib-crc32.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_dfa mulib_dfa_t;
typedef struct mulib_nfa_edge { size_t from, to; uint32_t lower, upper; int epsilon; } mulib_nfa_edge_t;
typedef struct mulib_nfa {
  size_t state_count, start;
  const unsigned char *accepting;
  const mulib_nfa_edge_t *edges;
  size_t edge_count;
} mulib_nfa_t;
typedef struct mulib_dfa_options {
  size_t max_states, max_edges, max_work; /**< Defaults 4096, 65536, 10000000. */
} mulib_dfa_options_t;
/** Return codes use mulib_fst_status_t. Missing edges reject, not an error. */
mulib_dfa_t *mulib_dfa_create(size_t max_states, size_t max_edges);
void mulib_dfa_destroy(mulib_dfa_t *);
int mulib_dfa_add_state(mulib_dfa_t *, int accepting, size_t *);
int mulib_dfa_add_range(mulib_dfa_t *, size_t, size_t, uint32_t, uint32_t);
int mulib_dfa_set_start(mulib_dfa_t *, size_t);
size_t mulib_dfa_start(const mulib_dfa_t *);
const mulib_fst_t *mulib_dfa_graph(const mulib_dfa_t *);
int mulib_dfa_accepts(const mulib_dfa_t *, const uint32_t *, size_t, size_t max_work);
/** Full epsilon closure and overlapping ranges supported. NULL on error;
 * optional status receives FST error. NFA limits: 4096 states, 65536 edges,
 * 64 MiB subset storage. No recursion; missing edges implicitly reject. */
mulib_dfa_t *mulib_dfa_from_nfa(const mulib_nfa_t *, const mulib_dfa_options_t *, int *status);
int mulib_dfa_serialize(const mulib_dfa_t *, mulib_buffer_t *);
int mulib_dfa_deserialize(const void *, size_t, const mulib_dfa_options_t *, mulib_dfa_t **);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_DFA) && !defined(MULIB_DFA_IMPLEMENTED)
#define MULIB_DFA_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
struct mulib_dfa { mulib_fst_t *graph; size_t start; };
mulib_dfa_t *mulib_dfa_create(size_t ns, size_t ne) {
  mulib_dfa_t *d = (mulib_dfa_t *)calloc(1, sizeof *d);
  if (!d) return NULL;
  d->graph = mulib_fst_create(ns, ne); d->start = MULIB_FST_NONE;
  if (!d->graph) { free(d); return NULL; } return d;
}
void mulib_dfa_destroy(mulib_dfa_t *d) { if (d) { mulib_fst_destroy(d->graph); free(d); } }
int mulib_dfa_add_state(mulib_dfa_t *d, int a, size_t *out) {
  int rc = d ? mulib_fst_add_state(d->graph, a, out) : MULIB_FST_INVALID;
  if (!rc && d->start == MULIB_FST_NONE) d->start = *out;
  return rc;
}
int mulib_dfa_add_range(mulib_dfa_t *d, size_t a, size_t b, uint32_t lo, uint32_t hi) {
  return d ? mulib_fst_add_range(d->graph, a, b, lo, hi, 0, 0, NULL) : MULIB_FST_INVALID;
}
int mulib_dfa_set_start(mulib_dfa_t *d, size_t s) {
  if (!d || s >= mulib_fst_state_count(d->graph)) return MULIB_FST_INVALID;
  d->start = s; return MULIB_FST_OK;
}
size_t mulib_dfa_start(const mulib_dfa_t *d) { return d ? d->start : MULIB_FST_NONE; }
const mulib_fst_t *mulib_dfa_graph(const mulib_dfa_t *d) { return d ? d->graph : NULL; }
int mulib_dfa_accepts(const mulib_dfa_t *d, const uint32_t *s, size_t n, size_t work) {
  mulib_fst_result_t result = {0}; int rc;
  if (!d) return MULIB_FST_INVALID;
  rc = mulib_fst_transduce(d->graph, d->start, s, n, work, &result, NULL);
  mulib_fst_result_free(&result); return rc;
}
static int mulib_dfa_tick_(size_t *work, size_t limit) { if (*work >= limit) return 0; ++*work; return 1; }
static int mulib_dfa_closure_(const mulib_nfa_t *n, uint64_t *set, size_t *work, size_t limit) {
  size_t i; int changed;
  do {
    changed = 0;
    for (i = 0; i < n->edge_count; ++i) {
      const mulib_nfa_edge_t *e = &n->edges[i];
      if (!mulib_dfa_tick_(work, limit)) return 0;
      if (e->epsilon && (set[e->from / 64] & (UINT64_C(1) << (e->from % 64))) &&
          !(set[e->to / 64] & (UINT64_C(1) << (e->to % 64)))) {
        set[e->to / 64] |= UINT64_C(1) << (e->to % 64); changed = 1;
      }
    }
  } while (changed);
  return 1;
}
static int mulib_dfa_u64cmp_(const void *a, const void *b) {
  uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b; return x < y ? -1 : x > y;
}
mulib_dfa_t *mulib_dfa_from_nfa(const mulib_nfa_t *n, const mulib_dfa_options_t *o, int *status) {
  size_t ms = o && o->max_states ? o->max_states : 4096;
  size_t me = o && o->max_edges ? o->max_edges : 65536;
  size_t limit = o && o->max_work ? o->max_work : 10000000;
  size_t words, count = 1, points = 0, unique, i, j, k, at, id, work = 0;
  uint64_t *sets = NULL, *tmp = NULL, *bounds = NULL;
  mulib_dfa_t *d = NULL;
  int rc = MULIB_FST_INVALID, nonempty, accepting;
  if (!n || !n->state_count || n->state_count > 4096 || n->start >= n->state_count ||
      !n->accepting || (!n->edges && n->edge_count) || n->edge_count > 65536) goto done;
  words = (n->state_count + 63) / 64;
  if (!ms || ms > 67108864u / sizeof(uint64_t) / words) { rc = MULIB_FST_LIMIT; goto done; }
  for (i = 0; i < n->edge_count; ++i) {
    const mulib_nfa_edge_t *e = &n->edges[i];
    if (e->from >= n->state_count || e->to >= n->state_count || (!e->epsilon && e->lower > e->upper)) goto done;
  }
  sets = (uint64_t *)calloc(ms * words, sizeof *sets);
  tmp = (uint64_t *)calloc(words, sizeof *tmp);
  bounds = (uint64_t *)malloc((2 * n->edge_count + 2) * sizeof *bounds);
  d = mulib_dfa_create(ms, me);
  if (!sets || !tmp || !bounds || !d) { rc = MULIB_FST_NOMEM; goto done; }
  bounds[points++] = 0; bounds[points++] = UINT64_C(4294967296);
  for (i = 0; i < n->edge_count; ++i) if (!n->edges[i].epsilon) {
    bounds[points++] = n->edges[i].lower; bounds[points++] = (uint64_t)n->edges[i].upper + 1;
  }
  qsort(bounds, points, sizeof *bounds, mulib_dfa_u64cmp_);
  unique = 0;
  for (i = 0; i < points; ++i) if (!i || bounds[i] != bounds[i - 1]) bounds[unique++] = bounds[i];
  sets[n->start / 64] |= UINT64_C(1) << (n->start % 64);
  if (!mulib_dfa_closure_(n, sets, &work, limit)) { rc = MULIB_FST_LIMIT; goto done; }
  for (at = 0; at < count; ++at) {
    uint64_t *current = sets + at * words;
    if (!at) {
      accepting = 0;
      for (k = 0; k < n->state_count; ++k) if (n->accepting[k] && (current[k / 64] & (UINT64_C(1) << (k % 64)))) accepting = 1;
      rc = mulib_dfa_add_state(d, accepting, &id); if (rc) goto done;
    }
    for (i = 0; i + 1 < unique; ++i) {
      memset(tmp, 0, words * sizeof *tmp);
      for (j = 0; j < n->edge_count; ++j) {
        const mulib_nfa_edge_t *e = &n->edges[j];
        if (!mulib_dfa_tick_(&work, limit)) { rc = MULIB_FST_LIMIT; goto done; }
        if (!e->epsilon && e->lower <= bounds[i] && e->upper >= bounds[i] &&
            (current[e->from / 64] & (UINT64_C(1) << (e->from % 64)))) tmp[e->to / 64] |= UINT64_C(1) << (e->to % 64);
      }
      nonempty = 0; for (k = 0; k < words; ++k) if (tmp[k]) nonempty = 1;
      if (!nonempty) continue;
      if (!mulib_dfa_closure_(n, tmp, &work, limit)) { rc = MULIB_FST_LIMIT; goto done; }
      for (j = 0; j < count; ++j) {
        for (k = 0; k < words; ++k) {
          if (!mulib_dfa_tick_(&work, limit)) { rc = MULIB_FST_LIMIT; goto done; }
          if (sets[j * words + k] != tmp[k]) break;
        }
        if (k == words) break;
      }
      if (j == count) {
        if (count == ms) { rc = MULIB_FST_LIMIT; goto done; }
        memcpy(sets + count * words, tmp, words * sizeof *tmp); ++count;
        accepting = 0;
        for (k = 0; k < n->state_count; ++k) if (n->accepting[k] && (tmp[k / 64] & (UINT64_C(1) << (k % 64)))) accepting = 1;
        rc = mulib_dfa_add_state(d, accepting, &id); if (rc) goto done;
      }
      rc = mulib_dfa_add_range(d, at, j, (uint32_t)bounds[i], (uint32_t)(bounds[i + 1] - 1));
      if (rc) goto done;
    }
  }
  rc = MULIB_FST_OK;
done:
  free(sets); free(tmp); free(bounds);
  if (rc) { mulib_dfa_destroy(d); d = NULL; }
  if (status) *status = rc;
  return d;
}
int mulib_dfa_serialize(const mulib_dfa_t *d, mulib_buffer_t *out) {
  mulib_buffer_t b = {0}; unsigned char header[12], crc[4]; int rc;
  if (!d || !out || d->start >= mulib_fst_state_count(d->graph)) return MULIB_FST_INVALID;
  memcpy(header, "MuDFA\1\0\0", 8); mulib_endian_store_le32(header + 8, (uint32_t)d->start);
  if (!mulib_buffer_append(&b, header, 12)) return MULIB_FST_NOMEM;
  rc = mulib_fst_serialize(d->graph, &b);
  if (!rc) {
    mulib_endian_store_le32(crc, mulib_crc32(b.data, b.length));
    if (!mulib_buffer_append(&b, crc, 4)) rc = MULIB_FST_NOMEM;
  }
  if (!rc && !mulib_buffer_append(out, b.data, b.length)) rc = MULIB_FST_NOMEM;
  mulib_buffer_free(&b); return rc;
}
int mulib_dfa_deserialize(const void *data, size_t n, const mulib_dfa_options_t *o, mulib_dfa_t **out) {
  const unsigned char *p = (const unsigned char *)data;
  mulib_dfa_t *d; size_t i; int rc;
  if (!p || !out || n < 16 || memcmp(p, "MuDFA\1\0\0", 8) ||
      mulib_endian_load_le32(p + n - 4) != mulib_crc32(p, n - 4)) return MULIB_FST_INVALID;
  d = (mulib_dfa_t *)calloc(1, sizeof *d); if (!d) return MULIB_FST_NOMEM;
  rc = mulib_fst_deserialize(p + 12, n - 16, o && o->max_states ? o->max_states : 4096,
      o && o->max_edges ? o->max_edges : 65536, &d->graph);
  if (!rc) {
    d->start = mulib_endian_load_le32(p + 8);
    if (d->start >= mulib_fst_state_count(d->graph)) rc = MULIB_FST_INVALID;
    for (i = 0; !rc && i < mulib_fst_edge_count(d->graph); ++i) {
      mulib_fst_edge_t e; mulib_fst_edge(d->graph, i, &e); if (e.emits) rc = MULIB_FST_INVALID;
    }
  }
  if (rc) { mulib_dfa_destroy(d); return rc; } *out = d; return MULIB_FST_OK;
}
#ifdef __cplusplus
}
#endif
#endif
