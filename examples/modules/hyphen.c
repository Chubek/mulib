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
int main(void) {
  mulib_hyphen_t *hyphen = mulib_hyphen_create(MULIB_CACHE_HATTRIE);
  mulib_hyphen_result_t result = {0};
  assert(hyphen && !mulib_hyphen_add_pattern(hyphen, "hy1phen"));
  assert(!mulib_hyphenate(hyphen, "hyphen", MULIB_HYPHEN_LIANG, &result));
  assert(result.count == 1 && result.offsets[0] == 2);
  mulib_hyphen_result_free(&result); mulib_hyphen_destroy(hyphen); return 0;
}
