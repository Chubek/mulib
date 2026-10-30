#define MULIB_IMPL_SOCKS5
#include "mulib-socks5.h"
#include <assert.h>
#include <string.h>
int
main (void)
{
  mulib_socks5_address_t a = { 3, { 0 }, 12, 443 }, decoded;
  unsigned char bytes[300];
  size_t n, used;
  unsigned command;
  memcpy (a.host, "example.test", 12);
  assert (mulib_socks5_encode (1, 0, &a, bytes, sizeof bytes, &n));
  assert (mulib_socks5_decode (bytes, n, 0, &command, &decoded, &used) == 1);
  assert (command == 1 && used == n && decoded.port == 443);
  assert (decoded.length == 12 && !memcmp (decoded.host, a.host, 12));
  return 0;
}
