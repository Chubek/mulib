/* Focused executable regression for mulib-soc-memorymap.h. */
#define MULIB_IMPL_SOC_MEMORYMAP
#include "mulib-soc-memorymap.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
int main(void) {
mulib_soc_memorymap_t m={0};uint32_t d;uint64_t o;assert(mulib_soc_memorymap_add(&m,0x1000,0x100,7));assert(!mulib_soc_memorymap_add(&m,0x1080,0x100,9));assert(mulib_soc_memorymap_lookup(&m,0x10ff,&d,&o)&&d==7&&o==255);assert(!mulib_soc_memorymap_lookup(&m,0x1100,&d,&o));
    puts("PASS mulib-soc-memorymap");
    return 0;
}
