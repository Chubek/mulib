/** @file mulib-imgtrackerd.h
 * @brief Compatibility entry point for archived OCI image cache tracking.
 * Define MULIB_IMPL_IMGTRACKERD and dependencies once; see its manual.
 */
#ifndef MULIB_IMGTRACKERD_COMPAT_H
#define MULIB_IMGTRACKERD_COMPAT_H
#include "mulib-memtable.h"
#include "mulib-bycutils.h"
#include "mulib-ociimage.h"
#include "../.deprecated/mulib-imgtrackerd.h"
#endif
#if defined(MULIB_IMPL_IMGTRACKERD)
#include "../.deprecated/mulib-imgtrackerd.h"
#endif
