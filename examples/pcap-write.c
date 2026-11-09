#include <stdint.h>
#include <stddef.h>
#define MULIB_IMPL_PCAP
#include "mulib-pcap.h"
#include <stdio.h>
int
main (void)
{
  uint8_t head[24];
  if (mulib_pcap_write_global (head, sizeof head, 65535, 1) < 0)
    return 1;
  FILE *f = fopen ("/tmp/mulib-demo.pcap", "wb");
  if (!f)
    return 1;
  size_t n = fwrite (head, 1, sizeof head, f);
  fclose (f);
  printf ("Wrote %zu PCAP bytes to /tmp/mulib-demo.pcap\n", n);
  return n != 24;
}
