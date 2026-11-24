/** @file mulib-spellcheck.h
 * @brief Configurable dictionary/affix/compound checking and five suggestion engines.
 * Define MULIB_IMPL_SPELLCHECK, CACHE, UTF8, BKTREE, BK_TREES, LEVENSHTEIN,
 * WEIGHTED_EDIT_DISTANCE, WEIGHED_EDIT_DISTANE, FST, FST_SERIALIZE and dependencies.
 * UTF-8 exact matching; edit distances/FST labels are bytes. No case folding.
 */
#ifndef MULIB_SPELLCHECK_H
#define MULIB_SPELLCHECK_H
#include "mulib-cache.h"
#include "mulib-utf8.h"
#include "mulib-bktree.h"
#include "mulib-weighted-edit-distance.h"
#include "mulib-fst.h"
#include "mulib-endian.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_spellcheck mulib_spellcheck_t;
enum mulib_spellcheck_check_flags {
  MULIB_SPELLCHECK_DICTIONARY = 1, MULIB_SPELLCHECK_AFFIX = 2,
  MULIB_SPELLCHECK_COMPOUND = 4, MULIB_SPELLCHECK_FST = 8
};
typedef enum mulib_spellcheck_algorithm {
  MULIB_SPELLCHECK_LEVENSHTEIN, MULIB_SPELLCHECK_WEIGHTED,
  MULIB_SPELLCHECK_TRIE, MULIB_SPELLCHECK_BKTREE, MULIB_SPELLCHECK_TRANSDUCER
} mulib_spellcheck_algorithm_t;
typedef enum mulib_spellcheck_affix_kind { MULIB_SPELLCHECK_PREFIX, MULIB_SPELLCHECK_SUFFIX } mulib_spellcheck_affix_kind_t;
enum mulib_spellcheck_status { MULIB_SPELLCHECK_INVALID = -1, MULIB_SPELLCHECK_MEMORY = -2, MULIB_SPELLCHECK_LIMIT = -3 };
typedef struct mulib_spellcheck_options {
  double max_distance, insertion, deletion, substitution;
  size_t max_results, max_work, max_compound_parts, min_component;
} mulib_spellcheck_options_t;
typedef struct mulib_spellcheck_suggestion { char *word; double distance; uint64_t frequency; } mulib_spellcheck_suggestion_t;
typedef struct mulib_spellcheck_result { mulib_spellcheck_suggestion_t *items; size_t count; } mulib_spellcheck_result_t;
/** Defaults: radius 2, costs 1, 16 results, 10000000 work, 8 compound parts,
 * 2 scalars per component. Explicit zero costs/radius are supported. */
mulib_spellcheck_options_t mulib_spellcheck_default_options(void);
mulib_spellcheck_t *mulib_spellcheck_create(mulib_cache_backend_t);
void mulib_spellcheck_destroy(mulib_spellcheck_t *);
/** Copy nonempty UTF-8 word (<=256 bytes), replace frequency. 0 success. */
int mulib_spellcheck_add_word(mulib_spellcheck_t *, const char *, uint64_t frequency);
/** Inverse affix transform: remove literal strip at the indicated edge, restore
 * literal text, then require a dictionary root. At most one prefix AND suffix
 * can combine; no implicit Hunspell flags/conditions. <=128 rules. */
int mulib_spellcheck_add_affix(mulib_spellcheck_t *, mulib_spellcheck_affix_kind_t,
    const char *strip, const char *restore, size_t minimum_stem_scalars);
/** Copy a byte-label acceptor/transducer. Outputs ignored for spelling. */
int mulib_spellcheck_set_fst(mulib_spellcheck_t *, const mulib_fst_t *, size_t start);
/** 1 correct, 0 incorrect, negative error. flags=0 selects dictionary only. */
int mulib_spellcheck_check(const mulib_spellcheck_t *, const char *, unsigned flags,
    const mulib_spellcheck_options_t *);
/** Suggestions sorted distance asc/frequency desc/byte lexicographic asc.
 * Owned output replaced only on success; returns 0 or negative error. Trie and
 * BK engines use unit byte distance and metric pruning, not weighted costs.
 * FST search traverses accepting byte paths with DP-row pruning (length<=256).
 * One shared budget includes index construction; limits discard partial results. */
int mulib_spellcheck_suggest(const mulib_spellcheck_t *, const char *, mulib_spellcheck_algorithm_t,
    const mulib_spellcheck_options_t *, mulib_spellcheck_result_t *);
void mulib_spellcheck_result_free(mulib_spellcheck_result_t *);
int mulib_spellcheck_serialize(const mulib_spellcheck_t *, mulib_buffer_t *);
int mulib_spellcheck_deserialize(const void *, size_t, mulib_cache_backend_t, mulib_spellcheck_t **);
/** Atomic UTF-8 text import, one word per nonempty LF/CRLF line, frequency 1. */
int mulib_spellcheck_dictionary_load(mulib_spellcheck_t *, const char *, size_t);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPELLCHECK) && !defined(MULIB_SPELLCHECK_IMPLEMENTED)
#define MULIB_SPELLCHECK_IMPLEMENTED
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_spellcheck_affix_ { mulib_spellcheck_affix_kind_t kind; size_t minimum; char strip[257], restore[257]; } mulib_spellcheck_affix_;
struct mulib_spellcheck {
  mulib_cache_t *data; mulib_cache_backend_t backend;
  mulib_spellcheck_affix_ affixes[128]; size_t affix_count;
  mulib_fst_t *fst; size_t start;
};
mulib_spellcheck_options_t mulib_spellcheck_default_options(void) {
  mulib_spellcheck_options_t o = {2, 1, 1, 1, 16, 10000000, 8, 2}; return o;
}
static int mulib_spellcheck_options_(const mulib_spellcheck_options_t *in, mulib_spellcheck_options_t *o) {
  *o = in ? *in : mulib_spellcheck_default_options();
  if (!o->max_results) o->max_results = 16;
  if (!o->max_work) o->max_work = 10000000;
  if (!o->max_compound_parts) o->max_compound_parts = 8;
  if (!o->min_component) o->min_component = 2;
  return isfinite(o->max_distance) && o->max_distance >= 0 && o->max_distance <= 4096 &&
    isfinite(o->insertion) && o->insertion >= 0 && isfinite(o->deletion) && o->deletion >= 0 &&
    isfinite(o->substitution) && o->substitution >= 0 && o->max_results <= 4096 &&
    o->max_compound_parts <= 256 && o->min_component <= 256;
}
static int mulib_spellcheck_text_(const char *s, size_t *n, int empty) {
  if (!s) return 0;
  *n = strlen(s); return (empty || *n) && *n <= 256 && mulib_utf8_validate((const unsigned char *)s, *n);
}
static int mulib_spellcheck_rc_(int rc) { return !rc ? 0 : rc == MULIB_KVDB_ENOMEM ? MULIB_SPELLCHECK_MEMORY : MULIB_SPELLCHECK_INVALID; }
static int mulib_spellcheck_tick_(size_t *work, size_t n, size_t limit) {
  if (n > limit - *work) return 0;
  *work += n; return 1;
}
mulib_spellcheck_t *mulib_spellcheck_create(mulib_cache_backend_t b) {
  mulib_spellcheck_t *s = (mulib_spellcheck_t *)calloc(1, sizeof *s);
  if (!s) return NULL;
  s->backend = b; s->data = mulib_cache_create(b);
  if (!s->data || mulib_cache_set(s->data, "@format", 7, "MuSpell1", 8)) { mulib_spellcheck_destroy(s); return NULL; }
  return s;
}
void mulib_spellcheck_destroy(mulib_spellcheck_t *s) { if (s) { mulib_cache_destroy(s->data); mulib_fst_destroy(s->fst); free(s); } }
int mulib_spellcheck_add_word(mulib_spellcheck_t *s, const char *word, uint64_t frequency) {
  unsigned char key[257], value[8]; size_t n;
  if (!s || !mulib_spellcheck_text_(word, &n, 0)) return MULIB_SPELLCHECK_INVALID;
  key[0] = 'D'; memcpy(key + 1, word, n); mulib_endian_store_le64(value, frequency);
  return mulib_spellcheck_rc_(mulib_cache_set(s->data, key, n + 1, value, 8));
}
static int mulib_spellcheck_lookup_(const mulib_spellcheck_t *s, const char *w, size_t n, uint64_t *frequency) {
  unsigned char key[257]; const void *v; size_t vn;
  if (!n || n > 256) return 0;
  key[0] = 'D'; memcpy(key + 1, w, n);
  if (mulib_cache_get(s->data, key, n + 1, &v, &vn) || vn != 8) return 0;
  if (frequency) *frequency = mulib_endian_load_le64(v);
  return 1;
}
int mulib_spellcheck_add_affix(mulib_spellcheck_t *s, mulib_spellcheck_affix_kind_t kind,
    const char *strip, const char *restore, size_t minimum) {
  unsigned char key[5], value[521]; size_t a, b; int rc;
  mulib_spellcheck_affix_ *rule;
  if (!s || s->affix_count == 128 || kind < MULIB_SPELLCHECK_PREFIX || kind > MULIB_SPELLCHECK_SUFFIX ||
      !mulib_spellcheck_text_(strip, &a, 0) || !mulib_spellcheck_text_(restore, &b, 1) || minimum > 256) return MULIB_SPELLCHECK_INVALID;
  key[0] = 'A'; mulib_endian_store_le32(key + 1, (uint32_t)s->affix_count);
  value[0] = (unsigned char)kind; mulib_endian_store_le32(value + 1, (uint32_t)minimum);
  mulib_endian_store_le32(value + 5, (uint32_t)a);
  memcpy(value + 9, strip, a); memcpy(value + 9 + a, restore, b);
  rc = mulib_cache_set(s->data, key, 5, value, 9 + a + b); if (rc) return mulib_spellcheck_rc_(rc);
  rule = &s->affixes[s->affix_count++]; rule->kind = kind; rule->minimum = minimum;
  memcpy(rule->strip, strip, a + 1); memcpy(rule->restore, restore, b + 1); return 0;
}
static size_t mulib_spellcheck_scalars_(const char *s, size_t n) {
  size_t i, count = 0; for (i = 0; i < n; ++i) if (((unsigned char)s[i] & 0xc0) != 0x80) ++count; return count;
}
static int mulib_spellcheck_affixed_(const mulib_spellcheck_t *s, const char *word, size_t n,
    int affix, size_t *work, size_t limit) {
  size_t a, b, pn, sn, pr, sr, length;
  char root[257];
  if (!mulib_spellcheck_tick_(work, 1, limit)) return MULIB_SPELLCHECK_LIMIT;
  if (mulib_spellcheck_lookup_(s, word, n, NULL)) return 1;
  if (!affix) return 0;
  for (a = 0; a <= s->affix_count; ++a) {
    const mulib_spellcheck_affix_ *p = a < s->affix_count ? &s->affixes[a] : NULL;
    if (p && p->kind != MULIB_SPELLCHECK_PREFIX) continue;
    pn = p ? strlen(p->strip) : 0; pr = p ? strlen(p->restore) : 0;
    if (pn > n || (p && memcmp(word, p->strip, pn))) continue;
    for (b = 0; b <= s->affix_count; ++b) {
      const mulib_spellcheck_affix_ *q = b < s->affix_count ? &s->affixes[b] : NULL;
      if (q && q->kind != MULIB_SPELLCHECK_SUFFIX) continue;
      if (!mulib_spellcheck_tick_(work, 1, limit)) return MULIB_SPELLCHECK_LIMIT;
      sn = q ? strlen(q->strip) : 0; sr = q ? strlen(q->restore) : 0;
      if ((!p && !q) || sn > n - pn || (q && memcmp(word + n - sn, q->strip, sn))) continue;
      length = pr + n - pn - sn + sr; if (!length || length > 256) continue;
      if (p) memcpy(root, p->restore, pr);
      memcpy(root + pr, word + pn, n - pn - sn);
      if (q) memcpy(root + pr + n - pn - sn, q->restore, sr);
      root[length] = 0;
      if ((p && mulib_spellcheck_scalars_(root, length) < p->minimum) ||
          (q && mulib_spellcheck_scalars_(root, length) < q->minimum)) continue;
      if (mulib_spellcheck_lookup_(s, root, length, NULL)) return 1;
    }
  }
  return 0;
}
int mulib_spellcheck_check(const mulib_spellcheck_t *s, const char *word, unsigned flags, const mulib_spellcheck_options_t *in) {
  mulib_spellcheck_options_t o; size_t n, work = 0, i, j; int rc;
  if (!s || !mulib_spellcheck_text_(word, &n, 0) || (flags & ~15u) || !mulib_spellcheck_options_(in, &o)) return MULIB_SPELLCHECK_INVALID;
  if (!flags) flags = MULIB_SPELLCHECK_DICTIONARY;
  if (flags & (MULIB_SPELLCHECK_DICTIONARY | MULIB_SPELLCHECK_AFFIX)) {
    rc = mulib_spellcheck_affixed_(s, word, n, flags & MULIB_SPELLCHECK_AFFIX, &work, o.max_work); if (rc) return rc;
  }
  if (flags & MULIB_SPELLCHECK_COMPOUND) {
    size_t parts[257], scalars[257];
    parts[0] = 0; scalars[0] = 0;
    for (i = 1; i <= n; ++i) { parts[i] = SIZE_MAX; scalars[i] = scalars[i - 1] + (((unsigned char)word[i - 1] & 0xc0) != 0x80); }
    for (i = 0; i < n; ++i) if (parts[i] < o.max_compound_parts) {
      for (j = i + 1; j <= n; ++j) {
        if (!i && j == n) continue;
        if ((j < n && ((unsigned char)word[j] & 0xc0) == 0x80) || scalars[j] - scalars[i] < o.min_component) continue;
        rc = mulib_spellcheck_affixed_(s, word + i, j - i, flags & MULIB_SPELLCHECK_AFFIX, &work, o.max_work);
        if (rc < 0) return rc;
        if (rc && parts[i] + 1 < parts[j]) parts[j] = parts[i] + 1;
      }
    }
    if (parts[n] >= 2 && parts[n] <= o.max_compound_parts) return 1;
  }
  if ((flags & MULIB_SPELLCHECK_FST) && s->fst) {
    uint32_t tokens[256]; mulib_fst_result_t r = {0};
    for (i = 0; i < n; ++i) tokens[i] = (unsigned char)word[i];
    if (work >= o.max_work) return MULIB_SPELLCHECK_LIMIT;
    rc = mulib_fst_transduce(s->fst, s->start, tokens, n, o.max_work - work, &r, NULL); mulib_fst_result_free(&r);
    if (rc == MULIB_FST_OK) return 1;
    if (rc == MULIB_FST_LIMIT) return MULIB_SPELLCHECK_LIMIT;
    if (rc == MULIB_FST_NOMEM) return MULIB_SPELLCHECK_MEMORY;
  }
  return 0;
}
void mulib_spellcheck_result_free(mulib_spellcheck_result_t *r) {
  size_t i; if (!r) return;
  for (i = 0; i < r->count; ++i) free(r->items[i].word);
  free(r->items); memset(r, 0, sizeof *r);
}
static int mulib_spellcheck_compare_(const void *aa, const void *bb) {
  const mulib_spellcheck_suggestion_t *a = (const mulib_spellcheck_suggestion_t *)aa, *b = (const mulib_spellcheck_suggestion_t *)bb;
  if (a->distance != b->distance) return a->distance < b->distance ? -1 : 1;
  if (a->frequency != b->frequency) return a->frequency > b->frequency ? -1 : 1;
  return strcmp(a->word, b->word);
}
typedef struct mulib_spellcheck_search_ {
  const mulib_spellcheck_t *s; const char *query; size_t length, work;
  mulib_spellcheck_options_t options; mulib_spellcheck_result_t result; int error;
} mulib_spellcheck_search_;
static int mulib_spellcheck_offer_(mulib_spellcheck_search_ *c, const char *word, size_t n, double d, uint64_t frequency) {
  mulib_spellcheck_suggestion_t candidate; size_t i;
  if (d > c->options.max_distance) return 1;
  for (i = 0; i < c->result.count; ++i) if (strlen(c->result.items[i].word) == n && !memcmp(c->result.items[i].word, word, n)) return 1;
  candidate.word = (char *)malloc(n + 1); if (!candidate.word) { c->error = MULIB_SPELLCHECK_MEMORY; return 0; }
  memcpy(candidate.word, word, n); candidate.word[n] = 0; candidate.distance = d; candidate.frequency = frequency;
  if (c->result.count < c->options.max_results) c->result.items[c->result.count++] = candidate;
  else if (mulib_spellcheck_compare_(&candidate, &c->result.items[c->result.count - 1]) < 0) {
    free(c->result.items[c->result.count - 1].word); c->result.items[c->result.count - 1] = candidate;
  } else free(candidate.word);
  qsort(c->result.items, c->result.count, sizeof *c->result.items, mulib_spellcheck_compare_); return 1;
}
static int mulib_spellcheck_trie_match_(const void *w, size_t n, uint64_t f, size_t d, void *u) {
  return mulib_spellcheck_offer_((mulib_spellcheck_search_ *)u, (const char *)w, n, (double)d, f);
}
static int mulib_spellcheck_bk_match_(const char *w, size_t d, void *u) {
  mulib_spellcheck_search_ *c = (mulib_spellcheck_search_ *)u; uint64_t frequency = 0; size_t n = strlen(w);
  mulib_spellcheck_lookup_(c->s, w, n, &frequency); return mulib_spellcheck_offer_(c, w, n, (double)d, frequency);
}
static int mulib_spellcheck_fst_walk_(mulib_spellcheck_search_ *c, size_t state, size_t depth,
    char word[257], size_t *rows) {
  mulib_fst_state_t st; size_t edge, token, i, min, *row = rows + depth * (c->length + 1);
  if (!mulib_spellcheck_tick_(&c->work, 1, c->options.max_work)) return MULIB_SPELLCHECK_LIMIT;
  if (mulib_fst_state(c->s->fst, state, &st)) return MULIB_SPELLCHECK_INVALID;
  if (st.accepting && depth && row[c->length] <= c->options.max_distance && mulib_utf8_validate((const unsigned char *)word, depth)) {
    uint64_t f = 0; mulib_spellcheck_lookup_(c->s, word, depth, &f);
    if (!mulib_spellcheck_offer_(c, word, depth, (double)row[c->length], f)) return c->error;
  }
  if (depth == 256) return 0;
  for (edge = st.first_edge; edge != MULIB_FST_NONE;) {
    mulib_fst_edge_t e;
    if (mulib_fst_edge(c->s->fst, edge, &e)) return MULIB_SPELLCHECK_INVALID;
    edge = e.next;
    if (!mulib_spellcheck_tick_(&c->work, 1, c->options.max_work)) return MULIB_SPELLCHECK_LIMIT;
    if (e.lower > 255 || !e.upper) continue;
    for (token = e.lower ? e.lower : 1; token <= e.upper && token <= 255; ++token) {
      size_t *next = row + c->length + 1; int rc;
      if (!mulib_spellcheck_tick_(&c->work, c->length + 1, c->options.max_work)) return MULIB_SPELLCHECK_LIMIT;
      min = next[0] = depth + 1;
      for (i = 1; i <= c->length; ++i) {
        size_t a = row[i] + 1, b = next[i - 1] + 1, d = row[i - 1] + (token != (unsigned char)c->query[i - 1]);
        next[i] = a < b ? a : b; if (d < next[i]) next[i] = d; if (next[i] < min) min = next[i];
      }
      if (min > c->options.max_distance) continue;
      word[depth] = (char)token; word[depth + 1] = 0;
      rc = mulib_spellcheck_fst_walk_(c, e.to, depth + 1, word, rows); if (rc) return rc;
    }
  }
  return 0;
}
int mulib_spellcheck_suggest(const mulib_spellcheck_t *s, const char *query, mulib_spellcheck_algorithm_t algorithm,
    const mulib_spellcheck_options_t *in, mulib_spellcheck_result_t *out) {
  mulib_spellcheck_search_ c; mulib_trie_t trie = {0}; mulib_bktree_t bk = {0}; size_t i, spent, result; int rc = 0;
  memset(&c, 0, sizeof c); c.s = s; c.query = query;
  if (!s || !out || !mulib_spellcheck_text_(query, &c.length, 1) || !mulib_spellcheck_options_(in, &c.options) ||
      algorithm < MULIB_SPELLCHECK_LEVENSHTEIN || algorithm > MULIB_SPELLCHECK_TRANSDUCER) return MULIB_SPELLCHECK_INVALID;
  c.result.items = (mulib_spellcheck_suggestion_t *)calloc(c.options.max_results, sizeof *c.result.items);
  if (!c.result.items) return MULIB_SPELLCHECK_MEMORY;
  if (algorithm == MULIB_SPELLCHECK_TRANSDUCER) {
    size_t *rows; char word[257];
    if (!s->fst) { rc = MULIB_SPELLCHECK_INVALID; goto done; }
    rows = (size_t *)malloc(257 * (c.length + 1) * sizeof *rows);
    if (!rows) { rc = MULIB_SPELLCHECK_MEMORY; goto done; }
    for (i = 0; i <= c.length; ++i) rows[i] = i;
    rc = mulib_spellcheck_fst_walk_(&c, s->start, 0, word, rows); free(rows); goto done;
  }
  for (i = 0; i < mulib_cache_size(s->data); ++i) {
    const void *key, *value; size_t kn, vn; char word[257]; uint64_t f;
    mulib_cache_at(s->data, i, &key, &kn, &value, &vn);
    if (!mulib_spellcheck_tick_(&c.work, 1, c.options.max_work)) { rc = MULIB_SPELLCHECK_LIMIT; goto done; }
    if (kn < 2 || *(const unsigned char *)key != 'D') continue;
    memcpy(word, (const unsigned char *)key + 1, kn - 1); word[kn - 1] = 0; f = mulib_endian_load_le64(value);
    if (algorithm == MULIB_SPELLCHECK_TRIE) {
      if (!mulib_spellcheck_tick_(&c.work, kn, c.options.max_work)) { rc = MULIB_SPELLCHECK_LIMIT; goto done; }
      if (!mulib_trie_set(&trie, word, kn - 1, f)) { rc = MULIB_SPELLCHECK_MEMORY; goto done; }
    } else if (algorithm == MULIB_SPELLCHECK_BKTREE) {
      if (c.work >= c.options.max_work) { rc = MULIB_SPELLCHECK_LIMIT; goto done; }
      if (!mulib_bktree_insert_bounded(&bk, word, c.options.max_work - c.work, &spent)) {
        rc = errno == ENOMEM ? MULIB_SPELLCHECK_MEMORY : MULIB_SPELLCHECK_LIMIT; goto done;
      }
      c.work += spent;
    } else {
      double d;
      if (!mulib_spellcheck_tick_(&c.work, kn * (c.length + 1), c.options.max_work)) { rc = MULIB_SPELLCHECK_LIMIT; goto done; }
      if (algorithm == MULIB_SPELLCHECK_WEIGHTED) d = mulib_weighted_edit_distance(query, c.length, word, kn - 1,
          c.options.insertion, c.options.deletion, c.options.substitution);
      else { size_t distance = mulib_levenshtein_distance(query, word); d = distance == SIZE_MAX ? NAN : (double)distance; }
      if (isnan(d)) { rc = MULIB_SPELLCHECK_MEMORY; goto done; }
      if (!mulib_spellcheck_offer_(&c, word, kn - 1, d, f)) { rc = c.error; goto done; }
    }
  }
  if (algorithm == MULIB_SPELLCHECK_TRIE || algorithm == MULIB_SPELLCHECK_BKTREE) {
    if (c.work >= c.options.max_work) { rc = MULIB_SPELLCHECK_LIMIT; goto done; }
    if (algorithm == MULIB_SPELLCHECK_TRIE)
      result = mulib_trie_approximate(&trie, query, c.length, (size_t)c.options.max_distance,
          c.options.max_work - c.work, mulib_spellcheck_trie_match_, &c, &spent);
    else result = mulib_bktree_search_bounded(&bk, query, (size_t)c.options.max_distance,
        c.options.max_work - c.work, mulib_spellcheck_bk_match_, &c, &spent);
    if (result == SIZE_MAX) rc = errno == ENOMEM ? MULIB_SPELLCHECK_MEMORY : MULIB_SPELLCHECK_LIMIT;
    if (c.error) rc = c.error;
  }
done:
  mulib_trie_free(&trie); mulib_bktree_free(&bk);
  if (!rc) { mulib_spellcheck_result_free(out); *out = c.result; }
  else mulib_spellcheck_result_free(&c.result);
  return rc;
}
int mulib_spellcheck_set_fst(mulib_spellcheck_t *s, const mulib_fst_t *f, size_t start) {
  mulib_buffer_t b = {0}; mulib_fst_t *copy = NULL; unsigned char meta[8]; int rc;
  if (!s || !f || start >= mulib_fst_state_count(f)) return MULIB_SPELLCHECK_INVALID;
  mulib_endian_store_le64(meta, (uint64_t)start);
  if (!mulib_buffer_append(&b, meta, 8)) return MULIB_SPELLCHECK_MEMORY;
  rc = mulib_fst_serialize(f, &b);
  if (!rc) rc = mulib_fst_deserialize(b.data + 8, b.length - 8, 0, 0, &copy);
  if (!rc) rc = mulib_cache_set(s->data, "@fst", 4, b.data, b.length);
  if (!rc) { mulib_fst_destroy(s->fst); s->fst = copy; s->start = start; copy = NULL; }
  mulib_fst_destroy(copy); mulib_buffer_free(&b); return mulib_spellcheck_rc_(rc);
}
int mulib_spellcheck_serialize(const mulib_spellcheck_t *s, mulib_buffer_t *b) {
  return s && b ? mulib_spellcheck_rc_(mulib_cache_serialize(s->data, b)) : MULIB_SPELLCHECK_INVALID;
}
int mulib_spellcheck_deserialize(const void *p, size_t n, mulib_cache_backend_t backend, mulib_spellcheck_t **out) {
  mulib_cache_t *data = NULL; mulib_spellcheck_t *s = NULL; const void *v; size_t vn, i; int rc;
  if (!out || n > 67108864) return MULIB_SPELLCHECK_INVALID;
  rc = mulib_cache_deserialize(p, n, backend, &data); if (rc) return mulib_spellcheck_rc_(rc);
  if (mulib_cache_get(data, "@format", 7, &v, &vn) || vn != 8 || memcmp(v, "MuSpell1", 8)) goto invalid;
  s = mulib_spellcheck_create(backend); if (!s) { rc = MULIB_SPELLCHECK_MEMORY; goto done; }
  for (i = 0; i < mulib_cache_size(data); ++i) {
    const void *key; size_t kn; char word[257];
    mulib_cache_at(data, i, &key, &kn, &v, &vn);
    if (kn == 7 && !memcmp(key, "@format", 7)) continue;
    if (kn == 4 && !memcmp(key, "@fst", 4)) {
      mulib_fst_t *f = NULL;
      if (vn < 8 || mulib_endian_load_le64(v) > SIZE_MAX) goto invalid;
      rc = mulib_fst_deserialize((const unsigned char *)v + 8, vn - 8, 0, 0, &f);
      if (rc) { rc = rc == MULIB_FST_NOMEM ? MULIB_SPELLCHECK_MEMORY : rc == MULIB_FST_LIMIT ? MULIB_SPELLCHECK_LIMIT : MULIB_SPELLCHECK_INVALID; goto done; }
      rc = mulib_spellcheck_set_fst(s, f, (size_t)mulib_endian_load_le64(v)); mulib_fst_destroy(f); if (rc) goto done; continue;
    }
    if (kn == 5 && *(const unsigned char *)key == 'A') {
      const unsigned char *bytes = (const unsigned char *)v; char strip[257], restore[257]; size_t length;
      if (mulib_endian_load_le32((const unsigned char *)key + 1) != s->affix_count || vn < 9 || vn > 521) goto invalid;
      length = mulib_endian_load_le32(bytes + 5);
      if (!length || length > 256 || length > vn - 9 || vn - 9 - length > 256 || memchr(bytes + 9, 0, vn - 9)) goto invalid;
      memcpy(strip, bytes + 9, length); strip[length] = 0;
      memcpy(restore, bytes + 9 + length, vn - 9 - length); restore[vn - 9 - length] = 0;
      rc = mulib_spellcheck_add_affix(s, (mulib_spellcheck_affix_kind_t)bytes[0], strip, restore, mulib_endian_load_le32(bytes + 1));
      if (rc) goto done;
    } else if (kn >= 2 && kn <= 257 && *(const unsigned char *)key == 'D' && vn == 8) {
      memcpy(word, (const unsigned char *)key + 1, kn - 1); word[kn - 1] = 0;
      if (memchr(word, 0, kn - 1)) goto invalid;
      rc = mulib_spellcheck_add_word(s, word, mulib_endian_load_le64(v)); if (rc) goto done;
    } else goto invalid;
  }
  mulib_cache_destroy(data); *out = s; return 0;
invalid:
  rc = MULIB_SPELLCHECK_INVALID;
done:
  mulib_cache_destroy(data); mulib_spellcheck_destroy(s); return rc;
}
int mulib_spellcheck_dictionary_load(mulib_spellcheck_t *s, const char *text, size_t n) {
  mulib_buffer_t b = {0}; mulib_spellcheck_t *copy = NULL; size_t i = 0; int rc;
  if (!s || (!text && n) || n > 16777216 || (n && memchr(text, 0, n))) return MULIB_SPELLCHECK_INVALID;
  rc = mulib_spellcheck_serialize(s, &b); if (!rc) rc = mulib_spellcheck_deserialize(b.data, b.length, s->backend, &copy);
  mulib_buffer_free(&b); if (rc) return rc;
  while (i < n) {
    char word[257]; size_t length = 0;
    while (i < n && text[i] != '\r' && text[i] != '\n') {
      if (length == 256) { rc = MULIB_SPELLCHECK_LIMIT; goto done; } word[length++] = text[i++];
    }
    while (i < n && (text[i] == '\r' || text[i] == '\n')) ++i;
    word[length] = 0;
    if (length && (rc = mulib_spellcheck_add_word(copy, word, 1)) != 0) goto done;
  }
  { mulib_spellcheck_t old = *s; *s = *copy; *copy = old; }
done:
  mulib_spellcheck_destroy(copy); return rc;
}
#ifdef __cplusplus
}
#endif
#endif
