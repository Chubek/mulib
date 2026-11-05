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
#include "mulib-hyphen.h"
#include <assert.h>
static void expect(mulib_hyphen_t *h, const char *word, mulib_hyphen_algorithm_t algo, const size_t *positions, size_t n) {
  mulib_hyphen_result_t r = {0}; size_t i;
  int rc = mulib_hyphenate(h, word, algo, &r);
  if (rc) fprintf(stderr, "hyphen %s algorithm %d: %d\n", word, (int)algo, rc);
  assert(!rc && r.count == n);
  for (i = 0; i < n; ++i) assert(r.offsets[i] == positions[i]);
  mulib_hyphen_result_free(&r);
}
static void test_cache(void) {
  int backend;
  { unsigned char unaligned[10]; uint64_t value = UINT64_C(0xfedcba9876543210);
    mulib_endian_store_le64(unaligned + 1, value); assert(mulib_endian_load_le64(unaligned + 1) == value);
    mulib_endian_store_be64(unaligned + 1, value); assert(mulib_endian_load_be64(unaligned + 1) == value);
    assert(!mulib_endian_load_le64(NULL) && !mulib_endian_load_be64(NULL));
  }
  for (backend = 0; backend < 3; ++backend) {
    mulib_cache_t *c = mulib_cache_create((mulib_cache_backend_t)backend), *d = NULL;
    mulib_buffer_t b = {0}; const void *v; size_t n;
    assert(c && !mulib_cache_set(c, "a\0b", 3, "x\0y", 3));
    assert(!mulib_cache_set(c, NULL, 0, NULL, 0));
    assert(!mulib_cache_get(c, "a\0b", 3, &v, &n) && n == 3 && !memcmp(v, "x\0y", 3));
    assert(!mulib_cache_serialize(c, &b));
    assert(!mulib_cache_deserialize(b.data, b.length, (mulib_cache_backend_t)((backend + 1) % 3), &d));
    assert(mulib_cache_size(d) == 2);
    assert(!mulib_cache_remove(d, "a\0b", 3));
    assert(mulib_cache_get(d, "a\0b", 3, &v, &n) == MULIB_KVDB_ENOTFOUND);
    assert(mulib_cache_commit(d) == MULIB_KVDB_EINVAL);
    { size_t i; char key[32]; unsigned char value[8];
      for (i = 200; i; --i) { snprintf(key, sizeof key, "word%zu", i); mulib_endian_store_le64(value, i); assert(!mulib_cache_set(d, key, strlen(key), value, 8)); }
      for (i = 1; i <= 200; ++i) {
        snprintf(key, sizeof key, "word%zu", i); assert(!mulib_cache_get(d, key, strlen(key), &v, &n) && n == 8 && mulib_endian_load_le64(v) == i);
        if (!(i % 2)) assert(!mulib_cache_remove(d, key, strlen(key)));
      }
      for (i = 201; i <= 300; ++i) { snprintf(key, sizeof key, "word%zu", i); mulib_endian_store_le64(value, i); assert(!mulib_cache_set(d, key, strlen(key), value, 8)); }
      for (i = 201; i <= 300; ++i) { snprintf(key, sizeof key, "word%zu", i); assert(!mulib_cache_get(d, key, strlen(key), &v, &n) && mulib_endian_load_le64(v) == i); }
    }
#if defined(__unix__) || defined(__APPLE__)
    {
      mulib_cache_t *disk = NULL;
      int rc = mulib_cache_save(c, "hyphen-cache.bin"); assert(rc >= 0);
      assert(!mulib_cache_open("hyphen-cache.bin", 0, MULIB_CACHE_HATTRIE, &disk));
      assert(mulib_cache_size(disk) == 2);
      assert(!mulib_cache_set(disk, "new", 3, "value", 5));
      assert(mulib_cache_commit(disk) >= 0); mulib_cache_destroy(disk);
      assert(!remove("hyphen-cache.bin"));
    }
#endif
    b.data[b.length - 1] ^= 1;
    { mulib_cache_t *bad = NULL; assert(mulib_cache_deserialize(b.data, b.length, MULIB_CACHE_TRIE, &bad) < 0 && !bad); }
    mulib_buffer_free(&b); mulib_cache_destroy(c); mulib_cache_destroy(d);
  }
}
static void test_dfa(void) {
  static const unsigned char accepting[] = {0, 0, 1, 0};
  static const mulib_nfa_edge_t edges[] = {
    {0, 1, 0, 0, 1}, {1, 0, 0, 0, 1}, {1, 2, 'a', 'c', 0},
    {0, 3, 'b', 'd', 0}, {3, 2, 'x', 'x', 0}, {2, 2, UINT32_MAX, UINT32_MAX, 0}
  };
  mulib_nfa_t nfa = {4, 0, accepting, edges, 0};
  mulib_dfa_t *d, *copy = NULL; mulib_buffer_t b = {0}; int rc;
  uint32_t a[] = {'a'}, bx[] = {'b', 'x'}, dx[] = {'d', 'x'}, max[] = {'c', UINT32_MAX};
  nfa.edge_count = sizeof edges / sizeof edges[0];
  d = mulib_dfa_from_nfa(&nfa, NULL, &rc); assert(d && rc == 0);
  assert(!mulib_dfa_accepts(d, a, 1, 0) && !mulib_dfa_accepts(d, bx, 2, 0));
  assert(!mulib_dfa_accepts(d, dx, 2, 0) && !mulib_dfa_accepts(d, max, 2, 0));
  assert(mulib_dfa_accepts(d, NULL, 0, 0) == MULIB_FST_NOMATCH);
  assert(!mulib_dfa_serialize(d, &b) && !mulib_dfa_deserialize(b.data, b.length, NULL, &copy));
  assert(!mulib_dfa_accepts(copy, bx, 2, 0));
  { mulib_dfa_options_t o = {1, 0, 0}; mulib_dfa_t *bad = mulib_dfa_from_nfa(&nfa, &o, &rc); assert(!bad && rc == MULIB_FST_LIMIT); }
  b.data[b.length - 1] ^= 1;
  { mulib_dfa_t *bad = NULL; assert(mulib_dfa_deserialize(b.data, b.length, NULL, &bad) && !bad); }
  mulib_buffer_free(&b); mulib_dfa_destroy(d); mulib_dfa_destroy(copy);
}
static uint32_t closure(const mulib_nfa_t *nfa, uint32_t states) {
  uint32_t old;
  do {
    size_t i; old = states;
    for (i = 0; i < nfa->edge_count; ++i) if (nfa->edges[i].epsilon && (states & (1u << nfa->edges[i].from))) states |= 1u << nfa->edges[i].to;
  } while (states != old);
  return states;
}
static void test_nfa_reference(void) {
  uint32_t seed = 713; size_t trial, i, code, length;
  for (trial = 0; trial < 100; ++trial) {
    unsigned char accepting[5]; mulib_nfa_edge_t edges[15]; mulib_nfa_t nfa = {5, 0, accepting, edges, 15}; mulib_dfa_t *d;
    for (i = 0; i < 5; ++i) { seed = seed * 1664525u + 1013904223u; accepting[i] = (unsigned char)(seed >> 31); }
    for (i = 0; i < 15; ++i) {
      seed = seed * 1664525u + 1013904223u; edges[i].from = seed % 5;
      seed = seed * 1664525u + 1013904223u; edges[i].to = seed % 5;
      seed = seed * 1664525u + 1013904223u; edges[i].lower = seed % 3; edges[i].upper = edges[i].lower;
      edges[i].epsilon = (seed >> 28) == 0;
    }
    d = mulib_dfa_from_nfa(&nfa, NULL, NULL); assert(d);
    for (length = 0; length <= 4; ++length) {
      size_t combinations = 1; for (i = 0; i < length; ++i) combinations *= 3;
      for (code = 0; code < combinations; ++code) {
        uint32_t tokens[4], states = closure(&nfa, 1); size_t x = code, j; int accepts = 0;
        for (i = 0; i < length; ++i) {
          uint32_t next = 0; tokens[i] = (uint32_t)(x % 3); x /= 3;
          for (j = 0; j < 15; ++j) if (!edges[j].epsilon && (states & (1u << edges[j].from)) && tokens[i] >= edges[j].lower && tokens[i] <= edges[j].upper) next |= 1u << edges[j].to;
          states = closure(&nfa, next);
        }
        for (i = 0; i < 5; ++i) if (accepting[i] && (states & (1u << i))) accepts = 1;
        assert(mulib_dfa_accepts(d, tokens, length, 0) == (accepts ? MULIB_FST_OK : MULIB_FST_NOMATCH));
      }
    }
    mulib_dfa_destroy(d);
  }
}
static void test_algorithms(void) {
  mulib_hyphen_t *h = mulib_hyphen_create(MULIB_CACHE_TRIE), *copy = NULL;
  mulib_buffer_t b = {0}; size_t two[] = {2}, four[] = {4}, six[] = {3, 6, 7};
  mulib_orhoconv_t *o = mulib_hyphen_conventions(h);
  assert(h && !mulib_hyphen_add_word(h, "hyphen", two, 1));
  expect(h, "hyphen", MULIB_HYPHEN_DICTIONARY, two, 1);
  assert(!mulib_hyphen_add_word(h, "nosplit", NULL, 0));
  expect(h, "nosplit", MULIB_HYPHEN_DICTIONARY, NULL, 0);
  assert(!mulib_hyphen_add_pattern(h, "hy1phen"));
  expect(h, "hyphen", MULIB_HYPHEN_LIANG, two, 1);
  assert(!mulib_hyphen_add_pattern(h, "hy2phen"));
  expect(h, "hyphen", MULIB_HYPHEN_LIANG, NULL, 0);
  assert(!mulib_hyphen_add_pattern(h, ".hy3"));
  expect(h, "hyphen", MULIB_HYPHEN_LIANG, two, 1);
  assert(!mulib_hyphen_add_morpheme(h, "un", MULIB_HYPHEN_PREFIX));
  assert(!mulib_hyphen_add_morpheme(h, "happy", MULIB_HYPHEN_ROOT));
  assert(!mulib_hyphen_add_morpheme(h, "ness", MULIB_HYPHEN_SUFFIX));
  { size_t breaks[] = {2, 7}; expect(h, "unhappyness", MULIB_HYPHEN_MORPHOLOGY, breaks, 2); }
  assert(!mulib_hyphen_add_morpheme(h, "rain", MULIB_HYPHEN_ROOT));
  assert(!mulib_hyphen_add_morpheme(h, "bow", MULIB_HYPHEN_ROOT));
  expect(h, "rainbow", MULIB_HYPHEN_MORPHOLOGY, four, 1);
  { size_t breaks[] = {2, 4}; expect(h, "banana", MULIB_HYPHEN_SYLLABLE, breaks, 2); }
  assert(!mulib_orhoconv_set_minima(o, 1, 1));
  { size_t breaks[] = {2, 4}; expect(h, "banana", MULIB_HYPHEN_SYLLABLE, breaks, 2); }
  assert(!mulib_orhoconv_add_token(o, MULIB_ORHOCONV_ONSET, "str"));
  { size_t breaks[] = {1}; expect(h, "astra", MULIB_HYPHEN_SYLLABLE, breaks, 1); }
  assert(!mulib_orhoconv_add_token(o, MULIB_ORHOCONV_PREFIX, "pre"));
  assert(!mulib_orhoconv_add_token(o, MULIB_ORHOCONV_SUFFIX, "ing"));
  expect(h, "pretesting", MULIB_HYPHEN_ORTHOGRAPHIC, six, 3);
  assert(!mulib_orhoconv_forbid(o, "ph", 1));
  { size_t breaks[] = {3}; assert(!mulib_hyphen_add_word(h, "alpha", breaks, 1)); expect(h, "alpha", MULIB_HYPHEN_DICTIONARY, NULL, 0); }
  { size_t breaks[] = {2}; assert(!mulib_hyphen_add_word(h, "écho", breaks, 1)); expect(h, "écho", MULIB_HYPHEN_DICTIONARY, breaks, 1); }
  assert(!mulib_hyphen_add_pattern(h, "é1cho")); expect(h, "écho", MULIB_HYPHEN_LIANG, two, 1);
  { unsigned char mask[] = {1, 1, 1, 1, 1};
    assert(!mulib_orhoconv_filter(o, "a\xcc\x81" "b", 4, mask) && !mask[1] && !mask[2]);
    assert(mulib_orhoconv_filter(o, "\xff", 1, mask) == MULIB_KVDB_EINVAL);
  }
  assert(!mulib_hyphen_serialize(h, &b));
  assert(!mulib_hyphen_deserialize(b.data, b.length, MULIB_CACHE_HATTRIE, &copy));
  expect(copy, "hyphen", MULIB_HYPHEN_LIANG, two, 1);
  expect(copy, "rainbow", MULIB_HYPHEN_MORPHOLOGY, four, 1);
  { size_t trial; uint32_t seed = 31; unsigned char *mutated = (unsigned char *)malloc(b.length); assert(mutated);
    for (trial = 0; trial < 300; ++trial) {
      mulib_hyphen_t *candidate = NULL; seed = seed * 1664525u + 1013904223u; memcpy(mutated, b.data, b.length);
      mutated[seed % (b.length - 4)] ^= (unsigned char)(1u << (seed % 8));
      mulib_endian_store_le32(mutated + b.length - 4, mulib_crc32(mutated, b.length - 4));
      (void)mulib_hyphen_deserialize(mutated, b.length, MULIB_CACHE_SORTED, &candidate); mulib_hyphen_destroy(candidate);
    } free(mutated);
  }
  assert(!mulib_hyphen_dictionary_load(copy, "dic-tion-ary\r\n", 14));
  { size_t breaks[] = {3, 7}; expect(copy, "dictionary", MULIB_HYPHEN_DICTIONARY, breaks, 2); }
  assert(mulib_hyphen_dictionary_load(copy, "good-word\nbad-", 14) != 0);
  { mulib_hyphen_result_t r = {0}; assert(mulib_hyphenate(copy, "goodword", MULIB_HYPHEN_DICTIONARY, &r) == MULIB_HYPHEN_NOMATCH); }
  mulib_hyphen_destroy(copy); mulib_buffer_free(&b); mulib_hyphen_destroy(h);
}
static void test_finite(void) {
  mulib_hyphen_t *h = mulib_hyphen_create(MULIB_CACHE_SORTED), *copy = NULL;
  mulib_fst_t *f = mulib_fst_create(0, 0); mulib_dfa_t *d = mulib_dfa_create(0, 0);
  mulib_buffer_t b = {0}; size_t q, i, positions[] = {2, 4};
  for (i = 0; i < 2; ++i) assert(!mulib_fst_add_state(f, 1, &q));
  assert(!mulib_fst_add_range(f, 0, 1, 'a', 'z', 0, 0, NULL));
  assert(!mulib_fst_add_range(f, 1, 0, 'a', 'z', 1, 1, NULL));
  assert(!mulib_hyphen_set_fst(h, f, 0));
  assert(!mulib_orhoconv_set_minima(mulib_hyphen_conventions(h), 1, 1));
  expect(h, "banana", MULIB_HYPHEN_FINITE_STATE, positions, 2);
  assert(!mulib_dfa_add_state(d, 0, &q) && !mulib_dfa_add_state(d, 1, &q));
  assert(!mulib_dfa_add_range(d, 0, 1, 'a', 'z') && !mulib_dfa_add_range(d, 1, 0, 'a', 'z'));
  assert(!mulib_hyphen_set_dfa(h, d)); /* only odd prefix lengths accepted */
  expect(h, "banana", MULIB_HYPHEN_FINITE_STATE, NULL, 0);
  assert(!mulib_hyphen_serialize(h, &b) && !mulib_hyphen_deserialize(b.data, b.length, MULIB_CACHE_TRIE, &copy));
  expect(copy, "banana", MULIB_HYPHEN_FINITE_STATE, NULL, 0);
  mulib_hyphen_destroy(copy); mulib_hyphen_destroy(h); mulib_fst_destroy(f); mulib_dfa_destroy(d); mulib_buffer_free(&b);
}
int main(void) { test_cache(); test_dfa(); test_nfa_reference(); test_algorithms(); test_finite(); puts("hyphen/cache/dfa/orhoconv passed"); return 0; }
