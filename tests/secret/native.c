#include <stdio.h>
#include "headers.h"

int main(void)
{
#if !defined(__unix__) && !defined(__APPLE__) && !defined(__linux__)
    /* The native entropy kernel intentionally fails on unsupported hosts. */
    return 77;
#else
    mulib_scr_provider provider;
    mulib_scr_native(&provider);
    int result = mulib_secret_selftest(&provider, MULIB_SCR_AES128_GCM);
    if (result)
        fprintf(stderr, "Native secret selftest failed at case %d\n", result);
    return result != 0;
#endif
}
