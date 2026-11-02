/** @file mulib-ocicontainermetadata.h
 * @brief Compatibility entry point for archived OCI metadata validation.
 * Define MULIB_IMPL_OCICONTAINERMETADATA and dependencies once; see its manual.
 */
#ifndef MULIB_OCICONTAINERMETADATA_COMPAT_H
#define MULIB_OCICONTAINERMETADATA_COMPAT_H
#include "mulib-json.h"
#include "mulib-ocicontainer.h"
#include "../.deprecated/mulib-ocicontainermetadata.h"
#endif
#if defined(MULIB_IMPL_OCICONTAINERMETADATA)
#include "../.deprecated/mulib-ocicontainermetadata.h"
#endif
