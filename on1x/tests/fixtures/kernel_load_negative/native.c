/* Test adapters. Only the three named functions belong in the dynamic symbol table. */
#define MULIB_IMPL_CLAMP
#include "mulib-clamp.h"

#define ADAPTER __attribute__((visibility("default")))

ADAPTER int on1x_kernel_native_add_i64(int64_t a, int64_t b, int64_t *out)
{
    *out = mulib_clamp_add_i64(a, b);
    return 0;
}

ADAPTER int on1x_kernel_native_i64(int64_t a, int64_t b, int64_t c, int64_t *out)
{
    if (b > c)
        return 1;
    *out = mulib_clamp_i64(a, b, c);
    return 0;
}

#ifndef OMIT_SUB
ADAPTER int on1x_kernel_native_sub_i64(int64_t a, int64_t b, int64_t *out)
{
    *out = mulib_clamp_sub_i64(a, b);
    return 0;
}
#endif

#ifdef WITH_INIT
/* Named only by the linker via -Wl,-init,negative_init. No imports or
 * relocations: this isolates the forbidden DT_INIT from other ELF rules. */
void negative_init(void)
{
}
#endif

#ifdef WITH_MARKER
#include <stdio.h>
#include <stdlib.h>

static void __attribute__((constructor)) record_load(void)
{
    const char *path = getenv("KERNEL_LOAD_NEGATIVE_MARKER");
    FILE *marker = path ? fopen(path, "w") : NULL;
    if (marker != NULL)
        fclose(marker);
}
#endif
