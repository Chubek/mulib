/** @file mulib-ocicontaineripc.h
 * @brief Compatibility entry point for archived OCI IPC framing.
 * Define MULIB_IMPL_OCICONTAINERIPC and dependencies once; see its manual.
 */
#ifndef MULIB_OCICONTAINERIPC_COMPAT_H
#define MULIB_OCICONTAINERIPC_COMPAT_H
#include "mulib-container2daemonproto.h"
#include "../.deprecated/mulib-ocicontaineripc.h"
#endif
#if defined(MULIB_IMPL_OCICONTAINERIPC)
#include "../.deprecated/mulib-ocicontaineripc.h"
#endif
