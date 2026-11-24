/** @file mulib-hyphen.h
 * @brief Dictionary, orthographic, Liang, morphology, FST/DFA and syllable hyphenation.
 * Define MULIB_IMPL_HYPHEN plus CACHE, ORHOCONV, DFA and their dependencies.
 * See manual/mulib-hyphen.md. Inputs are case-sensitive UTF-8, results byte offsets.
 */
#ifndef MULIB_HYPHEN_H
#define MULIB_HYPHEN_H
#include "mulib-cache.h"
#include "mulib-orhoconv.h"
#include "mulib-dfa.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_hyphen mulib_hyphen_t;
typedef enum mulib_hyphen_algorithm {
  MULIB_HYPHEN_DICTIONARY, MULIB_HYPHEN_ORTHOGRAPHIC, MULIB_HYPHEN_LIANG,
  MULIB_HYPHEN_MORPHOLOGY, MULIB_HYPHEN_FINITE_STATE, MULIB_HYPHEN_SYLLABLE
} mulib_hyphen_algorithm_t;
typedef enum mulib_hyphen_status {
  MULIB_HYPHEN_OK, MULIB_HYPHEN_NOMATCH, MULIB_HYPHEN_INVALID = -1,
  MULIB_HYPHEN_MEMORY = -2, MULIB_HYPHEN_LIMIT = -3
} mulib_hyphen_status_t;
typedef enum mulib_hyphen_morpheme {
  MULIB_HYPHEN_ROOT = 1, MULIB_HYPHEN_PREFIX = 2, MULIB_HYPHEN_SUFFIX = 4
} mulib_hyphen_morpheme_t;
typedef struct mulib_hyphen_result { size_t *offsets, count; } mulib_hyphen_result_t;
/** Dictionary is always indexed by HAT-trie; morphology and Liang patterns by
 * byte tries. backend selects the serializable model/cache index. */
mulib_hyphen_t *mulib_hyphen_create(mulib_cache_backend_t backend);
void mulib_hyphen_destroy(mulib_hyphen_t *);
/** Borrow owned conventions for configuration (default minima 2,2). */
mulib_orhoconv_t *mulib_hyphen_conventions(mulib_hyphen_t *);
/** Copy a word and explicit strictly increasing internal UTF-8 byte offsets.
 * Count zero is a known word with no permitted dictionary splits. */
int mulib_hyphen_add_word(mulib_hyphen_t *, const char *, const size_t *, size_t);
/** Read a UTF-8 text dictionary: one hy-phen-ated word per line; CRLF supported.
 * Empty lines skipped, no comments. Atomic across the entire input. */
int mulib_hyphen_dictionary_load(mulib_hyphen_t *, const char *, size_t);
/** TeX pattern syntax: ASCII weights 0..9 interleaved with letters, '.' word
 * anchors. Duplicate patterns merge componentwise maxima. No TeX macro parser. */
int mulib_hyphen_add_pattern(mulib_hyphen_t *, const char *);
/** A literal morpheme may have multiple flags. Analysis uses prefixes* roots+
 * suffixes*, choosing the fewest components; no inferred spelling alterations. */
int mulib_hyphen_add_morpheme(mulib_hyphen_t *, const char *, unsigned flags);
/** Copy machines. FST consumes Unicode scalars; output 1 marks a split AFTER
 * that symbol, output 0/no output does not. Final state must accept. DFA, if
 * present, filters candidates by accepting prefixes; alone, it proposes them. */
int mulib_hyphen_set_fst(mulib_hyphen_t *, const mulib_fst_t *, size_t start);
int mulib_hyphen_set_dfa(mulib_hyphen_t *, const mulib_dfa_t *);
/** Replace a zero-initialized/previous result only on success. Shared conventions
 * filter every algorithm. Empty words succeed with no splits. Work <=10000000,
 * word <=4096 bytes. NOMATCH preserves out. */
int mulib_hyphenate(const mulib_hyphen_t *, const char *, mulib_hyphen_algorithm_t, mulib_hyphen_result_t *);
void mulib_hyphen_result_free(mulib_hyphen_result_t *);
/** Complete model snapshots, including conventions and machines. Atomic append /
 * owned decode; the selected index is rebuilt, never pointer-deserialized. */
int mulib_hyphen_serialize(const mulib_hyphen_t *, mulib_buffer_t *);
int mulib_hyphen_deserialize(const void *, size_t, mulib_cache_backend_t, mulib_hyphen_t **);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HYPHEN) && !defined(MULIB_HYPHEN_IMPLEMENTED)
#define MULIB_HYPHEN_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
struct mulib_hyphen {
  mulib_cache_t *data;
  mulib_hattrie_t dictionary;
  mulib_trie_t patterns, morphology;
  mulib_orhoconv_t *conventions;
  mulib_fst_t *fst;
  mulib_dfa_t *dfa;
  size_t start;
  mulib_cache_backend_t backend;
};
static int mulib_hyphen_word_(const char *s, size_t *n) {
  if (!s) return 0;
  *n = strlen(s);
  return *n <= 4096 && mulib_utf8_validate((const unsigned char *)s, *n);
}
static int mulib_hyphen_status_(int rc) {
  return !rc ? MULIB_HYPHEN_OK : rc == MULIB_KVDB_ENOMEM ? MULIB_HYPHEN_MEMORY :
    rc == MULIB_KVDB_EOVERFLOW ? MULIB_HYPHEN_LIMIT : MULIB_HYPHEN_INVALID;
}
mulib_hyphen_t *mulib_hyphen_create(mulib_cache_backend_t b) {
  mulib_hyphen_t *h = (mulib_hyphen_t *)calloc(1, sizeof *h);
  if (!h) return NULL;
  h->data = mulib_cache_create(b); h->conventions = mulib_orhoconv_create(2, 2); h->backend = b;
  if (!h->data || !h->conventions) { mulib_hyphen_destroy(h); return NULL; } return h;
}
void mulib_hyphen_destroy(mulib_hyphen_t *h) {
  if (h) {
    mulib_cache_destroy(h->data); mulib_hattrie_free(&h->dictionary);
    mulib_trie_free(&h->patterns); mulib_trie_free(&h->morphology);
    mulib_orhoconv_destroy(h->conventions); mulib_fst_destroy(h->fst); mulib_dfa_destroy(h->dfa); free(h);
  }
}
mulib_orhoconv_t *mulib_hyphen_conventions(mulib_hyphen_t *h) { return h ? h->conventions : NULL; }
static int mulib_hyphen_store_(mulib_hyphen_t *h, unsigned char kind, const char *s, size_t n, const void *v, size_t vn) {
  unsigned char key[4097]; const void *old; size_t on;
  int present, rc;
  uint64_t flags = 0;
  key[0] = kind; if (n) memcpy(key + 1, s, n);
  present = mulib_cache_get(h->data, key, n + 1, &old, &on) == 0;
  if (kind == 'D') rc = mulib_hattrie_set(&h->dictionary, s, n, 1);
  else if (kind == 'P') rc = mulib_trie_set(&h->patterns, s, n, 1);
  else {
    mulib_trie_get(&h->morphology, s, n, &flags);
    rc = mulib_trie_set(&h->morphology, s, n, *(const unsigned char *)v);
  }
  if (!rc) return MULIB_HYPHEN_MEMORY;
  rc = mulib_cache_set(h->data, key, n + 1, v, vn);
  if (rc) {
    if (kind == 'M' && present) mulib_trie_set(&h->morphology, s, n, flags);
    else if (!present) {
      if (kind == 'D') mulib_hattrie_remove(&h->dictionary, s, n, NULL);
      else mulib_trie_remove(kind == 'P' ? &h->patterns : &h->morphology, s, n);
    }
  }
  return mulib_hyphen_status_(rc);
}
int mulib_hyphen_add_word(mulib_hyphen_t *h, const char *s, const size_t *splits, size_t count) {
  unsigned char mask[4097]; size_t n, i;
  if (!h || !mulib_hyphen_word_(s, &n) || (!splits && count) || count > n) return MULIB_HYPHEN_INVALID;
  memset(mask, 0, n + 1);
  for (i = 0; i < count; ++i) {
    size_t p = splits[i];
    if (!p || p >= n || (i && splits[i - 1] >= p) || ((unsigned char)s[p] & 0xc0) == 0x80) return MULIB_HYPHEN_INVALID;
    mask[p] = 1;
  }
  return mulib_hyphen_store_(h, 'D', s, n, mask, n + 1);
}
int mulib_hyphen_add_morpheme(mulib_hyphen_t *h, const char *s, unsigned flags) {
  size_t n; uint64_t old = 0; unsigned char value;
  if (!h || !mulib_hyphen_word_(s, &n) || !n || !flags || (flags & ~7u)) return MULIB_HYPHEN_INVALID;
  mulib_trie_get(&h->morphology, s, n, &old); value = (unsigned char)(old | flags);
  return mulib_hyphen_store_(h, 'M', s, n, &value, 1);
}
int mulib_hyphen_add_pattern(mulib_hyphen_t *h, const char *pattern) {
  char letters[4097]; unsigned char weights[4097]; unsigned char key[4097];
  size_t n, i, length = 0; const void *old; size_t on;
  int digit = 0;
  if (!h || !mulib_hyphen_word_(pattern, &n) || !n) return MULIB_HYPHEN_INVALID;
  memset(weights, 0, sizeof weights);
  for (i = 0; i < n; ++i) {
    unsigned char c = (unsigned char)pattern[i];
    if (c >= '0' && c <= '9') {
      if (digit || (i && (c & 0xc0) == 0x80)) return MULIB_HYPHEN_INVALID;
      weights[length] = (unsigned char)(c - '0'); digit = 1;
    } else { letters[length++] = (char)c; digit = 0; }
  }
  letters[length] = 0;
  if (!length || length > 4095 || !mulib_utf8_validate((const unsigned char *)letters, length)) return MULIB_HYPHEN_INVALID;
  for (i = 1; i < length; ++i) if (((unsigned char)letters[i] & 0xc0) == 0x80 && weights[i]) return MULIB_HYPHEN_INVALID;
  key[0] = 'P'; memcpy(key + 1, letters, length);
  if (!mulib_cache_get(h->data, key, length + 1, &old, &on)) {
    if (on != length + 1) return MULIB_HYPHEN_INVALID;
    for (i = 0; i <= length; ++i) if (((const unsigned char *)old)[i] > weights[i]) weights[i] = ((const unsigned char *)old)[i];
  }
  return mulib_hyphen_store_(h, 'P', letters, length, weights, length + 1);
}
static const mulib_trie_node_t *mulib_hyphen_child_(const mulib_trie_node_t *n, unsigned char c) {
  if (!n) return NULL;
  n = n->child; while (n && n->edge < c) n = n->next; return n && n->edge == c ? n : NULL;
}
static int mulib_hyphen_liang_(const mulib_hyphen_t *h, const char *s, size_t n, unsigned char *mask) {
  unsigned char text[4099], score[4100], key[4097]; size_t i, j, k, work = 0;
  memset(score, 0, sizeof score); text[0] = '.'; memcpy(text + 1, s, n); text[n + 1] = '.';
  for (i = 0; i < n + 2; ++i) {
    const mulib_trie_node_t *node = h->patterns.root;
    if ((text[i] & 0xc0) == 0x80) continue;
    for (j = i; j < n + 2 && (node = mulib_hyphen_child_(node, text[j])) != NULL; ++j) {
      const void *v; size_t vn;
      if (++work > 10000000) return MULIB_HYPHEN_LIMIT;
      if (!node->terminal) continue;
      key[0] = 'P'; memcpy(key + 1, text + i, j - i + 1);
      if (mulib_cache_get(h->data, key, j - i + 2, &v, &vn) || vn != j - i + 2) return MULIB_HYPHEN_INVALID;
      for (k = 0; k < vn; ++k) {
        if (++work > 10000000) return MULIB_HYPHEN_LIMIT;
        if (((const unsigned char *)v)[k] > score[i + k]) score[i + k] = ((const unsigned char *)v)[k];
      }
    }
  }
  for (i = 1; i < n; ++i) mask[i] = score[i + 1] & 1;
  return MULIB_HYPHEN_OK;
}
static int mulib_hyphen_morph_(const mulib_hyphen_t *h, const char *s, size_t n, unsigned char *mask) {
  size_t *cost, *previous, *phase, i, j, p, next, state, target, work = 0, best;
  cost = (size_t *)malloc(3 * (n + 1) * sizeof *cost);
  previous = (size_t *)malloc(3 * (n + 1) * sizeof *previous);
  phase = (size_t *)malloc(3 * (n + 1) * sizeof *phase);
  if (!cost || !previous || !phase) { free(cost); free(previous); free(phase); return MULIB_HYPHEN_MEMORY; }
  for (i = 0; i < 3 * (n + 1); ++i) cost[i] = SIZE_MAX;
  cost[0] = 0;
  for (i = 0; i < n; ++i) for (p = 0; p < 3; ++p) if (cost[3 * i + p] != SIZE_MAX) {
    const mulib_trie_node_t *node = h->morphology.root;
    for (j = i; j < n && (node = mulib_hyphen_child_(node, (unsigned char)s[j])) != NULL; ++j) {
      if (++work > 10000000) { free(cost); free(previous); free(phase); return MULIB_HYPHEN_LIMIT; }
      if (!node->terminal) continue;
      for (state = 0; state < 3; ++state) {
        unsigned flag = state == 0 ? MULIB_HYPHEN_PREFIX : state == 1 ? MULIB_HYPHEN_ROOT : MULIB_HYPHEN_SUFFIX;
        if (!(node->value & flag) || (state == 0 && p != 0) || (state == 1 && p == 2) || (state == 2 && p == 0)) continue;
        next = j + 1; target = 3 * next + state;
        if (cost[3 * i + p] + 1 < cost[target]) {
          cost[target] = cost[3 * i + p] + 1; previous[target] = i; phase[target] = p;
        }
      }
    }
  }
  p = cost[3 * n + 1] <= cost[3 * n + 2] ? 1 : 2; best = cost[3 * n + p]; i = n;
  if (best != SIZE_MAX) while (i) {
    size_t old = i; i = previous[3 * old + p]; p = phase[3 * old + p]; if (i) mask[i] = 1;
  }
  free(cost); free(previous); free(phase); return best == SIZE_MAX ? MULIB_HYPHEN_NOMATCH : MULIB_HYPHEN_OK;
}
static void mulib_hyphen_syllables_(const mulib_hyphen_t *h, const char *s, size_t n,
    const uint32_t *cp, const size_t *offsets, size_t count, unsigned char *mask, int orthographic) {
  size_t i, j, last = SIZE_MAX, onset, k;
  for (i = 0; i < count; ++i) if (mulib_orhoconv_is_vowel(h->conventions, cp[i])) {
    if (last != SIZE_MAX && i > last + 1) {
      onset = i - 1;
      /* Maximal registered onset, with one-consonant default. */
      for (j = last + 1; j < i; ++j)
        if (mulib_orhoconv_has_token(h->conventions, MULIB_ORHOCONV_ONSET, s + offsets[j], offsets[i] - offsets[j])) { onset = j; break; }
      mask[offsets[onset]] = 1;
    } else if (orthographic && last != SIZE_MAX && i == last + 1) mask[offsets[i]] = 1;
    last = i;
  }
  if (orthographic) {
    const mulib_kvdb_t *db = mulib_orhoconv_database(h->conventions);
    for (k = 0; k < mulib_kvdb_size(db); ++k) {
      const void *key, *v; size_t kn, vn;
      mulib_kvdb_at(db, k, &key, &kn, &v, &vn);
      if (kn <= 1 || kn - 1 >= n) continue;
      if (*(const unsigned char *)key == 'P' && !memcmp(s, (const unsigned char *)key + 1, kn - 1)) mask[kn - 1] = 1;
      if (*(const unsigned char *)key == 'S' && !memcmp(s + n - (kn - 1), (const unsigned char *)key + 1, kn - 1)) mask[n - (kn - 1)] = 1;
    }
  }
}
static int mulib_hyphen_finite_(const mulib_hyphen_t *h, const uint32_t *cp, const size_t *offsets,
    size_t count, unsigned char *mask) {
  size_t i, state = h->start, work = 0, spent; int emits, rc; uint32_t output;
  if (!h->fst && !h->dfa) return MULIB_HYPHEN_NOMATCH;
  if (h->fst) {
    mulib_fst_state_t final;
    for (i = 0; i < count; ++i) {
      if (work >= 10000000) return MULIB_HYPHEN_LIMIT;
      rc = mulib_fst_step(h->fst, state, cp[i], 10000000 - work, &state, &emits, &output, &spent); work += spent;
      if (rc) return rc == MULIB_FST_LIMIT ? MULIB_HYPHEN_LIMIT : MULIB_HYPHEN_NOMATCH;
      if (emits && output == 1) mask[offsets[i + 1]] = 1;
    }
    if (mulib_fst_state(h->fst, state, &final) || !final.accepting) return MULIB_HYPHEN_NOMATCH;
  }
  if (h->dfa) {
    const mulib_fst_t *graph = mulib_dfa_graph(h->dfa);
    state = mulib_dfa_start(h->dfa);
    for (i = 0; i < count; ++i) {
      mulib_fst_state_t current;
      if (work >= 10000000) return MULIB_HYPHEN_LIMIT;
      rc = mulib_fst_step(graph, state, cp[i], 10000000 - work, &state, &emits, &output, &spent); work += spent;
      if (rc == MULIB_FST_LIMIT) return MULIB_HYPHEN_LIMIT;
      if (rc) { for (; i < count; ++i) mask[offsets[i + 1]] = 0; break; }
      if (mulib_fst_state(graph, state, &current)) return MULIB_HYPHEN_INVALID;
      mask[offsets[i + 1]] = (unsigned char)(current.accepting && (!h->fst || mask[offsets[i + 1]]));
    }
  }
  return MULIB_HYPHEN_OK;
}
void mulib_hyphen_result_free(mulib_hyphen_result_t *r) { if (r) { free(r->offsets); memset(r, 0, sizeof *r); } }
int mulib_hyphenate(const mulib_hyphen_t *h, const char *s, mulib_hyphen_algorithm_t algorithm, mulib_hyphen_result_t *out) {
  size_t n, i, used, count = 0, found = 0, offsets[4097]; uint32_t cp[4096];
  unsigned char mask[4097], key[4097]; int rc = MULIB_HYPHEN_OK;
  mulib_hyphen_result_t result = {0};
  if (!h || !out || !mulib_hyphen_word_(s, &n) || algorithm < MULIB_HYPHEN_DICTIONARY || algorithm > MULIB_HYPHEN_SYLLABLE) return MULIB_HYPHEN_INVALID;
  memset(mask, 0, n + 1);
  for (i = 0; i < n; i += used) { offsets[count] = i; mulib_utf8_decode((const unsigned char *)s + i, n - i, &cp[count++], &used); }
  offsets[count] = n;
  if (n) switch (algorithm) {
  case MULIB_HYPHEN_DICTIONARY: {
    const void *v; size_t vn;
    if (!mulib_hattrie_get(&h->dictionary, s, n, NULL)) return MULIB_HYPHEN_NOMATCH;
    key[0] = 'D'; memcpy(key + 1, s, n);
    if (mulib_cache_get(h->data, key, n + 1, &v, &vn) || vn != n + 1) return MULIB_HYPHEN_INVALID;
    memcpy(mask, v, vn); break;
  }
  case MULIB_HYPHEN_LIANG: rc = mulib_hyphen_liang_(h, s, n, mask); break;
  case MULIB_HYPHEN_MORPHOLOGY: rc = mulib_hyphen_morph_(h, s, n, mask); break;
  case MULIB_HYPHEN_FINITE_STATE: rc = mulib_hyphen_finite_(h, cp, offsets, count, mask); break;
  default: mulib_hyphen_syllables_(h, s, n, cp, offsets, count, mask, algorithm == MULIB_HYPHEN_ORTHOGRAPHIC); break;
  }
  if (rc) return rc;
  rc = mulib_orhoconv_filter(h->conventions, s, n, mask); if (rc) return mulib_hyphen_status_(rc);
  for (i = 1; i < n; ++i) if (mask[i]) ++found;
  if (found) {
    result.offsets = (size_t *)malloc(found * sizeof *result.offsets); if (!result.offsets) return MULIB_HYPHEN_MEMORY;
    for (i = 1; i < n; ++i) if (mask[i]) result.offsets[result.count++] = i;
  }
  mulib_hyphen_result_free(out); *out = result; return MULIB_HYPHEN_OK;
}
int mulib_hyphen_set_fst(mulib_hyphen_t *h, const mulib_fst_t *f, size_t start) {
  mulib_buffer_t b = {0}; mulib_fst_t *copy = NULL; unsigned char meta[8]; size_t i; int rc;
  if (!h || !f || start >= mulib_fst_state_count(f)) return MULIB_HYPHEN_INVALID;
  for (i = 0; i < mulib_fst_edge_count(f); ++i) { mulib_fst_edge_t e; mulib_fst_edge(f, i, &e); if (e.emits && e.output > 1) return MULIB_HYPHEN_INVALID; }
  mulib_endian_store_le64(meta, (uint64_t)start);
  if (!mulib_buffer_append(&b, meta, 8)) return MULIB_HYPHEN_MEMORY;
  rc = mulib_fst_serialize(f, &b);
  if (!rc) rc = mulib_fst_deserialize(b.data + 8, b.length - 8, 0, 0, &copy);
  if (!rc) rc = mulib_cache_set(h->data, "@fst", 4, b.data, b.length);
  if (!rc) { mulib_fst_destroy(h->fst); h->fst = copy; h->start = start; copy = NULL; }
  mulib_fst_destroy(copy); mulib_buffer_free(&b); return rc ? (rc == MULIB_FST_NOMEM ? MULIB_HYPHEN_MEMORY : MULIB_HYPHEN_INVALID) : 0;
}
int mulib_hyphen_set_dfa(mulib_hyphen_t *h, const mulib_dfa_t *d) {
  mulib_buffer_t b = {0}; mulib_dfa_t *copy = NULL; int rc;
  if (!h || !d) return MULIB_HYPHEN_INVALID;
  rc = mulib_dfa_serialize(d, &b);
  if (!rc) rc = mulib_dfa_deserialize(b.data, b.length, NULL, &copy);
  if (!rc) rc = mulib_cache_set(h->data, "@dfa", 4, b.data, b.length);
  if (!rc) { mulib_dfa_destroy(h->dfa); h->dfa = copy; copy = NULL; }
  mulib_dfa_destroy(copy); mulib_buffer_free(&b); return rc ? (rc == MULIB_FST_NOMEM ? MULIB_HYPHEN_MEMORY : MULIB_HYPHEN_INVALID) : 0;
}
int mulib_hyphen_serialize(const mulib_hyphen_t *h, mulib_buffer_t *out) {
  mulib_buffer_t data = {0}, rules = {0}; mulib_cache_t *copy = NULL; int rc;
  if (!h || !out) return MULIB_HYPHEN_INVALID;
  rc = mulib_cache_serialize(h->data, &data);
  if (!rc) rc = mulib_cache_deserialize(data.data, data.length, MULIB_CACHE_SORTED, &copy);
  if (!rc) rc = mulib_orhoconv_serialize(h->conventions, &rules);
  if (!rc) rc = mulib_cache_set(copy, "@rules", 6, rules.data, rules.length);
  if (!rc) rc = mulib_cache_set(copy, "@format", 7, "MuHyphen1", 9);
  if (!rc) rc = mulib_cache_serialize(copy, out);
  mulib_cache_destroy(copy); mulib_buffer_free(&data); mulib_buffer_free(&rules); return mulib_hyphen_status_(rc);
}
int mulib_hyphen_deserialize(const void *p, size_t n, mulib_cache_backend_t backend, mulib_hyphen_t **out) {
  mulib_cache_t *data = NULL; mulib_hyphen_t *h = NULL; const void *v; size_t vn, i; int rc, have_rules = 0;
  if (!out || n > 67108864) return MULIB_HYPHEN_INVALID;
  rc = mulib_cache_deserialize(p, n, backend, &data); if (rc) return mulib_hyphen_status_(rc);
  if (mulib_cache_get(data, "@format", 7, &v, &vn) || vn != 9 || memcmp(v, "MuHyphen1", 9)) goto invalid;
  h = mulib_hyphen_create(backend); if (!h) { rc = MULIB_HYPHEN_MEMORY; goto done; }
  for (i = 0; i < mulib_cache_size(data); ++i) {
    const void *key; size_t kn, j; char word[4097]; unsigned char kind;
    mulib_cache_at(data, i, &key, &kn, &v, &vn);
    if (kn == 7 && !memcmp(key, "@format", 7)) continue;
    if (kn == 6 && !memcmp(key, "@rules", 6)) {
      mulib_orhoconv_t *rules = NULL;
      rc = mulib_orhoconv_deserialize(v, vn, &rules);
      if (rc) { rc = mulib_hyphen_status_(rc); goto done; }
      mulib_orhoconv_destroy(h->conventions); h->conventions = rules; have_rules = 1; continue;
    }
    if (kn == 4 && !memcmp(key, "@fst", 4)) {
      mulib_fst_t *f = NULL; size_t start;
      if (vn < 8 || mulib_endian_load_le64(v) > SIZE_MAX) goto invalid;
      start = (size_t)mulib_endian_load_le64(v);
      rc = mulib_fst_deserialize((const unsigned char *)v + 8, vn - 8, 0, 0, &f);
      if (rc) { rc = rc == MULIB_FST_NOMEM ? MULIB_HYPHEN_MEMORY : rc == MULIB_FST_LIMIT ? MULIB_HYPHEN_LIMIT : MULIB_HYPHEN_INVALID; goto done; }
      rc = mulib_hyphen_set_fst(h, f, start); mulib_fst_destroy(f); if (rc) goto done; continue;
    }
    if (kn == 4 && !memcmp(key, "@dfa", 4)) {
      mulib_dfa_t *d = NULL;
      rc = mulib_dfa_deserialize(v, vn, NULL, &d);
      if (rc) { rc = rc == MULIB_FST_NOMEM ? MULIB_HYPHEN_MEMORY : rc == MULIB_FST_LIMIT ? MULIB_HYPHEN_LIMIT : MULIB_HYPHEN_INVALID; goto done; }
      rc = mulib_hyphen_set_dfa(h, d); mulib_dfa_destroy(d); if (rc) goto done; continue;
    }
    if (kn < 1 || kn > 4097) goto invalid;
    kind = *(const unsigned char *)key; memcpy(word, (const unsigned char *)key + 1, kn - 1); word[kn - 1] = 0;
    if (memchr(word, 0, kn - 1) || !mulib_utf8_validate((const unsigned char *)word, kn - 1)) goto invalid;
    if (kind == 'D' || kind == 'P') {
      if (vn != kn || (kind == 'P' && kn < 2)) goto invalid;
      for (j = 0; j < vn; ++j) if (((const unsigned char *)v)[j] > (kind == 'D' ? 1 : 9) ||
          (j && j < kn - 1 && ((unsigned char)word[j] & 0xc0) == 0x80 && ((const unsigned char *)v)[j])) goto invalid;
      if (kind == 'D' && (((const unsigned char *)v)[0] || ((const unsigned char *)v)[vn - 1])) goto invalid;
      rc = mulib_hyphen_store_(h, kind, word, kn - 1, v, vn);
    } else if (kind == 'M' && kn > 1 && vn == 1 && *(const unsigned char *)v && !(*(const unsigned char *)v & ~7u))
      rc = mulib_hyphen_add_morpheme(h, word, *(const unsigned char *)v);
    else goto invalid;
    if (rc) goto done;
  }
  if (!have_rules) goto invalid;
  mulib_cache_destroy(data); *out = h; return MULIB_HYPHEN_OK;
invalid:
  rc = MULIB_HYPHEN_INVALID;
done:
  mulib_cache_destroy(data); mulib_hyphen_destroy(h); return rc;
}
int mulib_hyphen_dictionary_load(mulib_hyphen_t *h, const char *s, size_t n) {
  mulib_buffer_t snapshot = {0}; mulib_hyphen_t *copy = NULL; size_t i = 0; int rc;
  if (!h || (!s && n) || n > 16777216 || (n && memchr(s, 0, n))) return MULIB_HYPHEN_INVALID;
  rc = mulib_hyphen_serialize(h, &snapshot);
  if (!rc) rc = mulib_hyphen_deserialize(snapshot.data, snapshot.length, h->backend, &copy);
  mulib_buffer_free(&snapshot); if (rc) return rc;
  while (i < n) {
    char word[4097]; size_t length = 0, splits[4096], count = 0;
    while (i < n && s[i] != '\r' && s[i] != '\n') {
      if (s[i] == '-') {
        if (!length || count == 4096 || (count && splits[count - 1] == length)) { rc = MULIB_HYPHEN_INVALID; goto done; }
        splits[count++] = length; ++i;
      } else {
        if (length == 4096) { rc = MULIB_HYPHEN_LIMIT; goto done; }
        word[length++] = s[i++];
      }
    }
    while (i < n && (s[i] == '\r' || s[i] == '\n')) ++i;
    word[length] = 0;
    if (length && (rc = mulib_hyphen_add_word(copy, word, splits, count)) != 0) goto done;
  }
  { mulib_hyphen_t old = *h; *h = *copy; *copy = old; }
done:
  mulib_hyphen_destroy(copy); return rc;
}
#ifdef __cplusplus
}
#endif
#endif
