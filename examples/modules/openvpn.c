#define MULIB_IMPL_OPENVPN
#include "mulib-openvpn.h"
#include <assert.h>
int
main (void)
{
  const unsigned char encrypted[] = { 1, 2, 3 };
  mulib_openvpn_packet_t packet = { 9, 2, 42, encrypted, sizeof encrypted },
                         decoded;
  mulib_openvpn_replay_t replay = { 0 };
  unsigned char wire[16];
  size_t n;
  assert (mulib_openvpn_encode (&packet, wire, sizeof wire, &n));
  assert (mulib_openvpn_decode (wire, n, &decoded) == 1
          && decoded.peer_id == 42);
  /* Replay state is updated only after the application authenticates data. */
  assert (mulib_openvpn_replay_accept (&replay, 100));
  assert (!mulib_openvpn_replay_accept (&replay, 100));
  return 0;
}
