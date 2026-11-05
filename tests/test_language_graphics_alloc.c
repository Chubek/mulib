/* Inject each allocation failure, including dependencies and all decoders. */
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
static size_t allocations, fail_at = (size_t)-1, live;
static void *test_malloc(size_t n) { void *p; if (allocations++ == fail_at) return NULL; p = malloc(n); if (p) ++live; return p; }
static void *test_calloc(size_t n, size_t s) { void *p; if (allocations++ == fail_at) return NULL; p = calloc(n, s); if (p) ++live; return p; }
static void *test_realloc(void *old, size_t n) { void *p; int fresh = !old; if (allocations++ == fail_at) return NULL; p = realloc(old, n); if (p && fresh) ++live; return p; }
static void test_free(void *p) { if (p) { assert(live); --live; } free(p); }
#define malloc test_malloc
#define calloc test_calloc
#define realloc test_realloc
#define free test_free
#define MULIB_IMPL_HYPHEN
#define MULIB_IMPL_ORHOCONV
#define MULIB_IMPL_DFA
#define MULIB_IMPL_FST
#define MULIB_IMPL_FST_SERIALIZE
#define MULIB_IMPL_VEC
#define MULIB_IMPL_CACHE
#define MULIB_IMPL_KVDB
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_CRC32
#define MULIB_IMPL_VARINT
#define MULIB_IMPL_HATTRIE
#define MULIB_IMPL_HASH
#define MULIB_IMPL_TRIE
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_SPELLCHECK
#define MULIB_IMPL_BKTREE
#define MULIB_IMPL_BK_TREES
#define MULIB_IMPL_LEVENSHTEIN
#define MULIB_IMPL_WEIGHTED_EDIT_DISTANCE
#define MULIB_IMPL_WEIGHED_EDIT_DISTANE
#define MULIB_IMPL_CASSOWARY_SOLVER
#define MULIB_IMPL_2DGEOM
#define MULIB_IMPL_GEOMETRY
#define MULIB_IMPL_PARSEC
#include "mulib-hyphen.h"
#include "mulib-spellcheck.h"
#include "mulib-cassowary-solver.h"
#include "mulib-2dgeom.h"

static void cache_case(void) {
  mulib_cache_t *c = mulib_cache_create(MULIB_CACHE_HATTRIE), *copy = NULL;
  mulib_buffer_t b = {0}; const void *v; size_t n;
  if (!c || mulib_cache_set(c, "key", 3, "key", 3)) goto done;
  assert(!mulib_cache_get(c, "key", 3, &v, &n));
  if (mulib_cache_set(c, v, n, v, n) || mulib_cache_serialize(c, &b)) goto done;
  if (!mulib_cache_deserialize(b.data, b.length, MULIB_CACHE_TRIE, &copy)) {
    assert(!mulib_cache_get(copy, "key", 3, &v, &n)); assert(!mulib_cache_remove(copy, v, n));
  }
done:
  mulib_cache_destroy(c); mulib_cache_destroy(copy); mulib_buffer_free(&b);
}
static void hyphen_case(void) {
  mulib_hyphen_t *h = mulib_hyphen_create(MULIB_CACHE_TRIE), *copy = NULL;
  mulib_buffer_t b = {0}; mulib_hyphen_result_t r = {0}; size_t positions[] = {2};
  if (!h || mulib_hyphen_add_word(h, "hyphen", positions, 1) || mulib_hyphen_add_pattern(h, "hy1phen") ||
      mulib_hyphen_add_morpheme(h, "hyphen", 1) || mulib_hyphen_serialize(h, &b)) goto done;
  if (mulib_hyphen_deserialize(b.data, b.length, MULIB_CACHE_HATTRIE, &copy)) goto done;
  if (mulib_hyphenate(copy, "hyphen", MULIB_HYPHEN_LIANG, &r)) goto done;
  if (mulib_hyphenate(copy, "hyphen", MULIB_HYPHEN_MORPHOLOGY, &r)) goto done;
  (void)mulib_hyphen_dictionary_load(copy, "dic-tion-ary", sizeof "dic-tion-ary" - 1);
done:
  mulib_hyphen_result_free(&r); mulib_hyphen_destroy(h); mulib_hyphen_destroy(copy); mulib_buffer_free(&b);
}
static void spell_case(void) {
  mulib_spellcheck_t *s = mulib_spellcheck_create(MULIB_CACHE_HATTRIE), *copy = NULL;
  mulib_buffer_t b = {0}; mulib_spellcheck_result_t r = {0}; int algorithm;
  if (!s || mulib_spellcheck_add_word(s, "cat", 1) || mulib_spellcheck_add_word(s, "bat", 2) ||
      mulib_spellcheck_add_affix(s, MULIB_SPELLCHECK_SUFFIX, "s", "", 1)) goto done;
  for (algorithm = 0; algorithm < 4; ++algorithm)
    if (mulib_spellcheck_suggest(s, "cot", (mulib_spellcheck_algorithm_t)algorithm, NULL, &r)) goto done;
  if (mulib_spellcheck_serialize(s, &b) || mulib_spellcheck_deserialize(b.data, b.length, MULIB_CACHE_TRIE, &copy)) goto done;
  (void)mulib_spellcheck_dictionary_load(copy, "hello\nworld", sizeof "hello\nworld" - 1);
done:
  mulib_spellcheck_result_free(&r); mulib_spellcheck_destroy(s); mulib_spellcheck_destroy(copy); mulib_buffer_free(&b);
}
static void cass_case(void) {
  mulib_cassowary_solver_t *s = mulib_cassowary_create(NULL); size_t x, id;
  mulib_cassowary_term_t t;
  if (!s || mulib_cassowary_add_variable(s, &x)) goto done;
  t.variable = x; t.coefficient = 1;
  if (mulib_cassowary_add(s, &t, 1, MULIB_CASSOWARY_GE, 0, 0, &id) ||
      mulib_cassowary_add(s, &t, 1, MULIB_CASSOWARY_LE, 100, 0, &id) || mulib_cassowary_add_edit(s, x, 1000)) goto done;
  if (mulib_cassowary_suggest(s, x, 150) || mulib_cassowary_remove(s, id)) goto done;
  (void)mulib_cassowary_remove_edit(s, x);
done:
  mulib_cassowary_destroy(s);
}
static void geom_case(void) {
  mulib_2dgeom_path_t p = {0}, a = {0}, b = {0}; mulib_2dgeom_polyline_t flat = {0};
  const char text[] = "M0 0 C0 1 1 1 1 0 L2 0 Z";
  if (!mulib_2dgeom_path_parse(text, sizeof text - 1, &p, NULL)) goto done;
  if (!mulib_2dgeom_flatten(&mulib_2dgeom_path_at(&p, 0)->curve, 0.01, &flat)) goto done;
  (void)mulib_2dgeom_path_split(&p, 0, 0.5, &a, &b);
done:
  mulib_2dgeom_polyline_free(&flat); mulib_2dgeom_path_free(&p); mulib_2dgeom_path_free(&a); mulib_2dgeom_path_free(&b);
}
static void dfa_case(void) {
  unsigned char accepting[] = {0, 1}; mulib_nfa_edge_t edge = {0, 1, 'a', 'z', 0};
  mulib_nfa_t nfa = {2, 0, accepting, &edge, 1};
  mulib_dfa_t *d = mulib_dfa_from_nfa(&nfa, NULL, NULL), *copy = NULL;
  mulib_hyphen_t *h = NULL; mulib_spellcheck_t *s = NULL; mulib_buffer_t b = {0};
  if (!d || mulib_dfa_serialize(d, &b) || mulib_dfa_deserialize(b.data, b.length, NULL, &copy)) goto done;
  h = mulib_hyphen_create(MULIB_CACHE_HATTRIE); s = mulib_spellcheck_create(MULIB_CACHE_HATTRIE);
  if (!h || !s || mulib_hyphen_set_dfa(h, d) || mulib_hyphen_set_fst(h, mulib_dfa_graph(d), 0) ||
      mulib_spellcheck_set_fst(s, mulib_dfa_graph(d), 0)) goto done;
  { mulib_spellcheck_result_t result = {0}; (void)mulib_spellcheck_suggest(s, "b", MULIB_SPELLCHECK_TRANSDUCER, NULL, &result); mulib_spellcheck_result_free(&result); }
done:
  mulib_buffer_free(&b); mulib_dfa_destroy(d); mulib_dfa_destroy(copy); mulib_hyphen_destroy(h); mulib_spellcheck_destroy(s);
}
int main(void) {
  void (*cases[])(void) = {cache_case, hyphen_case, spell_case, cass_case, geom_case, dfa_case}; size_t c, i, count;
  for (c = 0; c < sizeof cases / sizeof cases[0]; ++c) {
    allocations = 0; fail_at = SIZE_MAX; cases[c](); assert(!live); count = allocations;
    for (i = 0; i < count; ++i) { allocations = 0; fail_at = i; cases[c](); assert(!live); }
  }
  puts("language/graphics allocation failure cleanup passed"); return 0;
}
