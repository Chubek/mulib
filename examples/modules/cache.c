#define MULIB_IMPL_CACHE
#define MULIB_IMPL_KVDB
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_CRC32
#define MULIB_IMPL_VARINT
#define MULIB_IMPL_HATTRIE
#define MULIB_IMPL_HASH
#define MULIB_IMPL_TRIE
#include "mulib-cache.h"
#include <assert.h>
int main(void) {
  mulib_cache_t *cache = mulib_cache_create(MULIB_CACHE_HATTRIE), *copy = NULL;
  mulib_buffer_t bytes = {0}; const void *value; size_t length;
  assert(cache && !mulib_cache_set(cache, "model", 5, "data", 4));
  assert(!mulib_cache_serialize(cache, &bytes));
  assert(!mulib_cache_deserialize(bytes.data, bytes.length, MULIB_CACHE_TRIE, &copy));
  assert(!mulib_cache_get(copy, "model", 5, &value, &length) && length == 4 && !memcmp(value, "data", 4));
  mulib_cache_destroy(cache); mulib_cache_destroy(copy); mulib_buffer_free(&bytes); return 0;
}
