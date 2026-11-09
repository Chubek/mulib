/** @file mulib-browserscreenshot.h
 * @brief Validate screenshot dimensions and output raster buffer size.
 * C11 single-header implementation. Define MULIB_IMPL_BROWSERSCREENSHOT in one translation unit.
 */
#ifndef MULIB_BROWSERSCREENSHOT_H
#define MULIB_BROWSERSCREENSHOT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_browserscreenshot_size(size_t width,size_t height,size_t channels,size_t *out);

#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_BROWSERSCREENSHOT) && !defined(MULIB_IMPL_BROWSERSCREENSHOT_IMPLEMENTED)
#define MULIB_IMPL_BROWSERSCREENSHOT_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#ifdef __cplusplus
extern "C" {
#endif

int mulib_browserscreenshot_size(size_t w,size_t h,size_t c,size_t *out){if(!out||!w||!h||(c!=3&&c!=4)||w>32768||h>32768||w>SIZE_MAX/h/c)return 0;*out=w*h*c;return 1;}

#ifdef __cplusplus
}
#endif
#endif
