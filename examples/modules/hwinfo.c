#define MULIB_IMPL_HWINFO
#include "mulib-hwinfo.h"
#include <stdio.h>
int main(void) {
    mulib_hwinfo_cpu_t cpu;
    mulib_hwinfo_cache_t cache;
    if (!mulib_hwinfo_cpu(NULL, NULL, &cpu))
        printf("CPU: %s %s family %u model %u\n", cpu.vendor, cpu.brand,
               (unsigned)cpu.family, (unsigned)cpu.model);
    else puts("Native CPUID unavailable");
    if (cpu.vendor[0] == 'G' && !mulib_hwinfo_cache(NULL, NULL, 4, 0, &cache))
        printf("First Intel cache: %llu bytes\n", (unsigned long long)cache.bytes);
    return 0;
}
