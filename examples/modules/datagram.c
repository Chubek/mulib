#define MULIB_IMPL_FILECTL
#define MULIB_IMPL_DATAGRAM
#include "mulib-datagram.h"
#include <assert.h>
#include <string.h>
int
main (void)
{
#if defined(__unix__) || defined(__APPLE__)
  mulib_datagram_t a = MULIB_DATAGRAM_INIT, b = MULIB_DATAGRAM_INIT;
  mulib_datagram_address_t bind_address = { { 127, 0, 0, 1 }, 0, 4, 0 }, aa,
                           bb, sender;
  char bytes[8];
  size_t n;
  assert (!mulib_datagram_open (&a, &bind_address, 0, &aa));
  assert (!mulib_datagram_open (&b, &bind_address, 0, &bb));
  assert (!mulib_datagram_send (&a, &bb, "hello", 5));
  assert (!mulib_datagram_receive (&b, &sender, bytes, sizeof bytes, &n));
  assert (n == 5 && sender.port == aa.port && !memcmp (bytes, "hello", 5));
  assert (!mulib_datagram_close (&a));
  assert (!mulib_datagram_close (&b));
#endif
  return 0;
}
