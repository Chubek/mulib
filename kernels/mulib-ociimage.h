/** @file mulib-ociimage.h
 * @brief Compatibility entry point for archived OCI manifest validation.
 * Define MULIB_IMPL_OCIIMAGE and dependencies once; see its manual.
 */
#ifndef MULIB_OCIIMAGE_COMPAT_H
#define MULIB_OCIIMAGE_COMPAT_H
#include "mulib-json.h"
#include "../.deprecated/mulib-ociimage.h"
#endif
#if defined(MULIB_IMPL_OCIIMAGE)
#include "../.deprecated/mulib-ociimage.h"
#endif
