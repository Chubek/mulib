#define MULIB_IMPL_NET_CLI_FORMAT
#include "mulib-net-cli-format.h"
#include <assert.h>
int
main (void)
{
  char text[64];
  const uint8_t ip[4] = { 127, 0, 0, 1 };
  assert (mulib_net_cli_format_ipv4 (ip, text, sizeof text) > 0);
  puts (text);
  assert (mulib_net_cli_format_json ("an interface\"name", text, sizeof text)
          > 0);
  puts (text);
  return 0;
}
