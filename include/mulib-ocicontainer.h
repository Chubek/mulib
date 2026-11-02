/** @file mulib-ocicontainer.h
 * @brief Compatibility entry point for the archived OCI lifecycle model.
 * Retained because public container adapters still depend on this API.
 * Define MULIB_IMPL_OCICONTAINER once; see manual/mulib-ocicontainer.md.
 */
#ifndef MULIB_OCICONTAINER_COMPAT_H
#define MULIB_OCICONTAINER_COMPAT_H
#include "../.deprecated/mulib-ocicontainer.h"
#endif
#if defined(MULIB_IMPL_OCICONTAINER)
#include "../.deprecated/mulib-ocicontainer.h"
#endif
