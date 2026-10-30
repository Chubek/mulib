#define MULIB_IMPL_SOCKS5
#define MULIB_IMPL_V2RAYPROTO
#include "mulib-v2rayproto.h"
#include <assert.h>
#include <string.h>
int
main (void)
{
  mulib_v2rayproto_request_t request = { 0 }, decoded;
  unsigned char wire[300];
  size_t n, used;
  request.uuid[0] = 1;
  request.command = 1;
  request.address.type = 1;
  request.address.length = 4;
  request.address.host[0] = 127;
  request.address.host[3] = 1;
  request.address.port = 443;
  assert (mulib_v2rayproto_encode (&request, wire, sizeof wire, &n));
  assert (mulib_v2rayproto_decode (wire, n, &decoded, &used) == 1
          && used == n);
  assert (decoded.address.port == 443
          && !memcmp (decoded.uuid, request.uuid, 16));
  return 0;
}
