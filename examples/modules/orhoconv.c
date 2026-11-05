#define MULIB_IMPL_ORHOCONV
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_KVDB
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_CRC32
#define MULIB_IMPL_VARINT
#include "mulib-orhoconv.h"
#include <assert.h>
int main(void) {
  mulib_orhoconv_t *rules = mulib_orhoconv_create(1, 1);
  unsigned char mask[] = {1, 1, 1, 1, 1, 1};
  assert(rules && !mulib_orhoconv_forbid(rules, "ph", 1));
  assert(!mulib_orhoconv_filter(rules, "alpha", 5, mask));
  assert(!mask[0] && !mask[3] && !mask[5] && mask[2]);
  mulib_orhoconv_destroy(rules); return 0;
}
