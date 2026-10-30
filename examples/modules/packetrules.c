#define MULIB_IMPL_PACKETFITER
#define MULIB_IMPL_PACKETRULES
#include "mulib-packetrules.h"
#include <assert.h>
int
main (void)
{
  const unsigned char packet[28]
      = { 0x45, 0, 0,   28, 0, 0, 0,    0,    64, 17, 0, 0, 127, 0,
          0,    1, 127, 0,  0, 1, 0x12, 0x34, 0,  53, 0, 8, 0,   0 };
  mulib_packetrules_rule_t rule = { 0 };
  size_t matched;
  rule.family = 4;
  rule.protocol = 17;
  rule.ports = 1;
  rule.source_high = 65535;
  rule.destination_low = rule.destination_high = 53;
  rule.action = 1;
  assert (
      mulib_packetrules_apply (&rule, 1, 0, packet, sizeof packet, &matched)
      == 1);
  assert (matched == 0);
  return 0;
}
