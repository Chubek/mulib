/* Generated, standalone MuLIB add-on symbol and API test: lldp */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_LLDP
#include "mulib-lldp.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_lldp_foreach;
    assert (p);
  }
  puts ("PASS lldp");
  return 0;
}
