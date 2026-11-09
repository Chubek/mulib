/* Generated, standalone MuLIB add-on symbol and API test: pcap */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_PCAP
#include "mulib-pcap.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_pcap_write_global;
    assert (p);
  }
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_pcap_write_record;
    assert (p);
  }

  uint8_t b[64];
  assert (mulib_pcap_write_global (b, sizeof b, 65535, 1) == 24);
  assert (b[0] == 0xd4 && b[1] == 0xc3);
  puts ("PASS pcap");
  return 0;
}
