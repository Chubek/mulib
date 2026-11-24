/** @file mulib-ocicontainervolume.h
 * @brief Compatibility entry point for archived OCI volume validation.
 * Define MULIB_IMPL_OCICONTAINERVOLUME and dependencies once; see its manual.
 */
#ifndef MULIB_OCICONTAINERVOLUME_COMPAT_H
#define MULIB_OCICONTAINERVOLUME_COMPAT_H
#include "mulib-vfs.h"
#include "../.deprecated/mulib-ocicontainervolume.h"
#endif
#if defined(MULIB_IMPL_OCICONTAINERVOLUME)
#include "../.deprecated/mulib-ocicontainervolume.h"
#endif
