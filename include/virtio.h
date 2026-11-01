/** @file virtio.h
 * @brief Public Virtio contract entry point; canonical declarations are in mulib-virtio.h.
 * Implementation switches remain MULIB_IMPL_VIRTIO, MULIB_IMPL_VMMDEV,
 * and MULIB_IMPL_ENDIAN. Backend and frontend have their own headers.
 */
#ifndef MULIB_PUBLIC_VIRTIO_H
#define MULIB_PUBLIC_VIRTIO_H
#include "mulib-virtio.h"
#endif
#if defined(MULIB_IMPL_VIRTIO)
#include "mulib-virtio.h"
#endif
