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
int main(void) {
  mulib_spellcheck_t *speller = mulib_spellcheck_create(MULIB_CACHE_HATTRIE);
  mulib_spellcheck_result_t result = {0};
  assert(speller && !mulib_spellcheck_add_word(speller, "hello", 10));
  assert(!mulib_spellcheck_add_affix(speller, MULIB_SPELLCHECK_SUFFIX, "s", "", 2));
  assert(mulib_spellcheck_check(speller, "hellos", MULIB_SPELLCHECK_AFFIX, NULL) == 1);
  assert(!mulib_spellcheck_suggest(speller, "helo", MULIB_SPELLCHECK_TRIE, NULL, &result));
  assert(result.count == 1 && !strcmp(result.items[0].word, "hello"));
  mulib_spellcheck_result_free(&result); mulib_spellcheck_destroy(speller); return 0;
}
