/* Generated, standalone MuLIB add-on symbol and API test: net-cli-format */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_NET_CLI_FORMAT
#include "mulib-net-cli-format.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_net_cli_format_ipv4;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_net_cli_format_json;
    assert (p);
  }

  uint8_t ip[4] = { 127, 0, 0, 1 };
  char b[32];
  assert (mulib_net_cli_format_ipv4 (ip, b, sizeof b) > 0);
  assert (!strcmp (b, "127.0.0.1"));
  assert (mulib_net_cli_format_json ("a\"b", b, sizeof b) > 0);
  puts ("PASS net-cli-format");
  return 0;
}
