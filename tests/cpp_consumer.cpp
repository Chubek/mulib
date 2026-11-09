#include "mulib-uip-udp.h"
#include <cassert>
#include <cstdint>
int main() {
  std::uint8_t s[4] = {10,0,0,1}, d[4] = {10,0,0,2}, packet[64];
  std::size_t n = 0;
  assert(mulib_uip_udp_pack(packet,sizeof packet,s,d,123,456,"hello",5,&n) == 0);
  mulib_uip_udp_datagram_t decoded{};
  assert(mulib_uip_udp_parse(s,d,packet,n,&decoded) == 0);
  return decoded.dst_port == 456 && decoded.length == 5 ? 0 : 1;
}
