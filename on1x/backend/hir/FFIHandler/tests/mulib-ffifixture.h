#ifndef MULIB_FFIFIXTURE_H
#define MULIB_FFIFIXTURE_H

#include <stdint.h>
int64_t mulib_ffifixture_bump(int64_t x);
int64_t mulib_ffifixture_floor(int64_t x);
int64_t mulib_ffifixture_zero(void);

#if defined(MULIB_IMPL_FFIFIXTURE)
int64_t mulib_ffifixture_bump(int64_t x)
{
    return x < INT64_MAX ? x + 1 : x;
}

int64_t mulib_ffifixture_floor(int64_t x)
{
    return x < 0 ? 0 : x;
}

int64_t mulib_ffifixture_zero(void)
{
    return 0;
}
#endif
#endif
