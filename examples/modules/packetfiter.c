#define MULIB_IMPL_PACKETFITER
#include "mulib-packetfiter.h"
#include <assert.h>
int
main (void)
{
  const unsigned char packet[28]
      = { 0x45, 0, 0,   28, 0, 0, 0,    0,    64, 17, 0, 0, 127, 0,
          0,    1, 127, 0,  0, 1, 0x12, 0x34, 0,  53, 0, 8, 0,   0 };
  mulib_packetfiter_view_t view;
  assert (mulib_packetfiter_parse (packet, sizeof packet, &view) == 1);
  assert (view.family == 4 && view.has_ports && view.destination_port == 53);
  return 0;
}
