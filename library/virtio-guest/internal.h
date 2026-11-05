#ifndef MULIB_VIRTIO_GUEST_INTERNAL_H
#define MULIB_VIRTIO_GUEST_INTERNAL_H
#include "mulib-virtio-guest.h"
#include <string.h>
#define MULIB_VIRTIO_INTERNAL

MULIB_VIRTIO_INTERNAL int mulib_virtio_guest_workspace_(mulib_virtio_guest_t *g, size_t ni, size_t no);
MULIB_VIRTIO_INTERNAL int mulib_virtio_guest_prepared_(mulib_virtio_guest_t *g, unsigned q, size_t ni, size_t no, size_t *used);
#endif
