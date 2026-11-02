/** @file mulib-ocicontainervfs.h
 * @brief Compatibility entry point for archived OCI VFS policy adapters.
 * Define MULIB_IMPL_OCICONTAINERVFS and dependencies once; see its manual.
 */
#ifndef MULIB_OCICONTAINERVFS_COMPAT_H
#define MULIB_OCICONTAINERVFS_COMPAT_H
#include "mulib-rootfs.h"
#include "../.deprecated/mulib-ocicontainervfs.h"
#endif
#if defined(MULIB_IMPL_OCICONTAINERVFS)
#include "../.deprecated/mulib-ocicontainervfs.h"
#endif
