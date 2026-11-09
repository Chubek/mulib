/* Generated, standalone MuLIB add-on symbol and API test: tracepath */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_TRACEPATH
#include "mulib-tracepath.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_tracepath_probe;
    assert (p);
  }
  puts ("PASS tracepath");
  return 0;
}
