#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_VMMDEV
#include "mulib-vmmdev.h"
#include <assert.h>

int main(void)
{
  unsigned char bytes[40];
  size_t size = sizeof bytes;
  mulib_vmmdev_message_t request = {0}, decoded;
  request.opcode = MULIB_VMMDEV_PROBE; request.device = 1; request.token = 123;
  assert(!mulib_vmmdev_encode(&request, bytes, &size));
  assert(!mulib_vmmdev_decode(bytes, size, &decoded));
  assert(decoded.device == 1 && decoded.token == 123 && !decoded.length);
  return 0;
}
