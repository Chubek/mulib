#define MULIB_IMPL_PCAP
#include "mulib-pcap.h"
#include <assert.h>
int main(void) { uint8_t bytes[64]; size_t used = 0; assert(mulib_pcap_write_global(bytes, sizeof bytes, 65535, 1) == 24); assert(!mulib_pcap_write_record(bytes + 24, sizeof bytes - 24, 0, 1, "data", 4, &used)); assert(used == 20); return 0; }
