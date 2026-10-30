#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_GIT
#define MULIB_IMPL_BITTORRENT
#include "mulib-bittorrent.h"
#include <assert.h>
#include <string.h>
int
main (void)
{
  mulib_bittorrent_handshake_t h = { { 0 }, { 1 }, { 2 } }, decoded;
  mulib_bittorrent_message_t keepalive = { 255, NULL, 0 }, message;
  unsigned char wire[68];
  size_t n, used;
  assert (mulib_bittorrent_handshake_encode (&h, wire, sizeof wire));
  assert (mulib_bittorrent_handshake_decode (wire, sizeof wire, &decoded)
          == 1);
  assert (!memcmp (decoded.info_hash, h.info_hash, 20));
  assert (mulib_bittorrent_encode (&keepalive, wire, sizeof wire, &n));
  assert (mulib_bittorrent_decode (wire, n, &message, &used) == 1
          && message.id == 255);
  return 0;
}
