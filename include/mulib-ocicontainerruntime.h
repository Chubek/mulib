/** @file mulib-ocicontainerruntime.h
 * @brief Compatibility entry point for archived OCI runtime adapters.
 * Define MULIB_IMPL_OCICONTAINERRUNTIME and dependencies once; see its manual.
 */
#ifndef MULIB_OCICONTAINERRUNTIME_COMPAT_H
#define MULIB_OCICONTAINERRUNTIME_COMPAT_H
#include "mulib-ocicontainer.h"
#include "../.deprecated/mulib-ocicontainerruntime.h"
#endif
#if defined(MULIB_IMPL_OCICONTAINERRUNTIME)
#include "../.deprecated/mulib-ocicontainerruntime.h"
#endif
