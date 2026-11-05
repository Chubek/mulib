#define MULIB_IMPL_SPELLCHECK
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
#define MULIB_IMPL_FST
#define MULIB_IMPL_FST_SERIALIZE
#define MULIB_IMPL_VEC
#define MULIB_IMPL_BKTREE
#define MULIB_IMPL_BK_TREES
#define MULIB_IMPL_LEVENSHTEIN
#define MULIB_IMPL_WEIGHTED_EDIT_DISTANCE
#define MULIB_IMPL_WEIGHED_EDIT_DISTANE
#include "mulib-spellcheck.h"
#include <assert.h>
static double cost(mulib_weighted_edit_operation_t op, uint32_t a, uint32_t b, void *u) {
  (void)u; return op == MULIB_WEIGHTED_SUBSTITUTE && a == 'q' && b == 'k' ? 0.25 : 1;
}
static void test_distances(void) {
  uint32_t a[] = {'q'}, b[] = {'k'}, u[] = {0x1f600}, v[] = {0x1f601}; double d = -1;
  assert(mulib_weighted_edit_distance_ex(a, 1, b, 1, cost, NULL, 0, &d) && d == 0.25);
  assert(mulib_weighted_edit_distance_ex(u, 1, v, 1, cost, NULL, 0, &d) && d == 1);
  assert(!mulib_weighted_edit_distance_ex(a, 1, b, 1, cost, NULL, 1, &d) && d == 1);
  assert(mulib_weighted_edit_distance("kitten", 6, "sitting", 7, 1, 1, 1) == 3);
  assert(isnan(mulib_weighted_edit_distance("a", 1, "b", 1, -1, 1, 1)));
}
static void suggestions(mulib_spellcheck_t *s) {
  mulib_spellcheck_result_t r = {0}; mulib_spellcheck_options_t o = mulib_spellcheck_default_options(); int algo;
  o.max_distance = 1;
  for (algo = MULIB_SPELLCHECK_LEVENSHTEIN; algo <= MULIB_SPELLCHECK_BKTREE; ++algo) {
    assert(!mulib_spellcheck_suggest(s, "cot", (mulib_spellcheck_algorithm_t)algo, &o, &r));
    assert(r.count == 2 && !strcmp(r.items[0].word, "cat") && !strcmp(r.items[1].word, "cut"));
    assert(r.items[0].frequency == 10 && r.items[0].distance == 1);
  }
  o.max_results = 1;
  assert(!mulib_spellcheck_suggest(s, "cot", MULIB_SPELLCHECK_TRIE, &o, &r) && r.count == 1);
  o.max_work = 1;
  assert(mulib_spellcheck_suggest(s, "cot", MULIB_SPELLCHECK_TRIE, &o, &r) == MULIB_SPELLCHECK_LIMIT);
  assert(r.count == 1 && !strcmp(r.items[0].word, "cat")); mulib_spellcheck_result_free(&r);
}
int main(void) {
  mulib_spellcheck_t *s = mulib_spellcheck_create(MULIB_CACHE_HATTRIE), *copy = NULL;
  mulib_buffer_t b = {0}; mulib_fst_t *f = mulib_fst_create(0, 0); size_t q, i;
  mulib_spellcheck_result_t r = {0}; mulib_spellcheck_options_t o = mulib_spellcheck_default_options();
  test_distances(); assert(s);
  assert(!mulib_spellcheck_add_word(s, "cat", 10) && !mulib_spellcheck_add_word(s, "cut", 2));
  assert(!mulib_spellcheck_add_word(s, "city", 3) && !mulib_spellcheck_add_word(s, "happy", 2));
  assert(!mulib_spellcheck_add_word(s, "rain", 1) && !mulib_spellcheck_add_word(s, "bow", 1));
  assert(!mulib_spellcheck_add_word(s, "café", 1));
  assert(mulib_spellcheck_check(s, "cat", 0, NULL) == 1 && mulib_spellcheck_check(s, "cot", 0, NULL) == 0);
  assert(mulib_spellcheck_check(s, "café", 0, NULL) == 1 && mulib_spellcheck_check(s, "Cat", 0, NULL) == 0);
  assert(!mulib_spellcheck_add_affix(s, MULIB_SPELLCHECK_PREFIX, "un", "", 2));
  assert(!mulib_spellcheck_add_affix(s, MULIB_SPELLCHECK_SUFFIX, "ies", "y", 2));
  assert(!mulib_spellcheck_add_affix(s, MULIB_SPELLCHECK_SUFFIX, "s", "", 2));
  assert(mulib_spellcheck_check(s, "unhappy", MULIB_SPELLCHECK_AFFIX, NULL) == 1);
  assert(mulib_spellcheck_check(s, "cities", MULIB_SPELLCHECK_AFFIX, NULL) == 1);
  assert(mulib_spellcheck_check(s, "uncities", MULIB_SPELLCHECK_AFFIX, NULL) == 1);
  assert(mulib_spellcheck_check(s, "rainbow", MULIB_SPELLCHECK_COMPOUND, NULL) == 1);
  assert(mulib_spellcheck_check(s, "rainbows", MULIB_SPELLCHECK_COMPOUND | MULIB_SPELLCHECK_AFFIX, NULL) == 1);
  assert(mulib_spellcheck_check(s, "unknown", 7, NULL) == 0);
  suggestions(s);
  for (i = 0; i < 4; ++i) assert(!mulib_fst_add_state(f, i == 3, &q));
  assert(!mulib_fst_add_range(f, 0, 1, 'b', 'b', 0, 0, NULL));
  assert(!mulib_fst_add_range(f, 0, 1, 'c', 'c', 0, 0, NULL));
  assert(!mulib_fst_add_range(f, 1, 2, 'a', 'a', 0, 0, NULL));
  assert(!mulib_fst_add_range(f, 2, 3, 't', 't', 0, 0, NULL));
  assert(!mulib_fst_add_range(f, 3, 3, 's', 's', 0, 0, NULL));
  assert(!mulib_spellcheck_set_fst(s, f, 0)); mulib_fst_destroy(f);
  assert(mulib_spellcheck_check(s, "bats", MULIB_SPELLCHECK_FST, NULL) == 1);
  o.max_distance = 1;
  assert(!mulib_spellcheck_suggest(s, "cot", MULIB_SPELLCHECK_TRANSDUCER, &o, &r));
  assert(r.count == 1 && !strcmp(r.items[0].word, "cat")); mulib_spellcheck_result_free(&r);
  assert(!mulib_spellcheck_serialize(s, &b));
  assert(!mulib_spellcheck_deserialize(b.data, b.length, MULIB_CACHE_TRIE, &copy));
  suggestions(copy); assert(mulib_spellcheck_check(copy, "cities", MULIB_SPELLCHECK_AFFIX, NULL) == 1);
  { size_t trial; uint32_t seed = 47; unsigned char *mutated = (unsigned char *)malloc(b.length); assert(mutated);
    for (trial = 0; trial < 300; ++trial) {
      mulib_spellcheck_t *candidate = NULL; seed = seed * 1664525u + 1013904223u; memcpy(mutated, b.data, b.length);
      mutated[seed % (b.length - 4)] ^= (unsigned char)(1u << (seed % 8));
      mulib_endian_store_le32(mutated + b.length - 4, mulib_crc32(mutated, b.length - 4));
      (void)mulib_spellcheck_deserialize(mutated, b.length, MULIB_CACHE_TRIE, &candidate); mulib_spellcheck_destroy(candidate);
    } free(mutated);
  }
  assert(mulib_spellcheck_check(copy, "bats", MULIB_SPELLCHECK_FST, NULL) == 1);
  assert(!mulib_spellcheck_dictionary_load(copy, "hello\r\nworld\n", sizeof "hello\r\nworld\n" - 1));
  assert(mulib_spellcheck_check(copy, "hello", 0, NULL) == 1);
  b.data[b.length / 2] ^= 1;
  { mulib_spellcheck_t *bad = NULL; assert(mulib_spellcheck_deserialize(b.data, b.length, MULIB_CACHE_SORTED, &bad) < 0 && !bad); }
  assert(mulib_spellcheck_check(s, "\xff", 0, NULL) == MULIB_SPELLCHECK_INVALID);
  mulib_buffer_free(&b); mulib_spellcheck_destroy(s); mulib_spellcheck_destroy(copy);
  puts("spellcheck/weighted-edit/bktree passed"); return 0;
}
