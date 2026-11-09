#define MULIB_IMPL_ICMP
#include "mulib-icmp.h"
#include <assert.h>
int main(void) { uint8_t bytes[32]; size_t used = 0; assert(!mulib_icmp_echo(bytes, sizeof bytes, 8, 1, 1, "hello", 5, &used)); assert(!mulib_icmp_checksum(bytes, used)); return 0; }
