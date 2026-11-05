#include "internal.h"

int mulib_virtio_guest_nsm_request(mulib_virtio_guest_t *g, unsigned queue,
    const void *input, size_t input_size, void *output, size_t *capacity)
{
  if (!g || g->device_id != MULIB_VIRTIO_NSM || input_size > MULIB_VIRTIO_NSM_REQUEST_MAX || !capacity
      || *capacity > MULIB_VIRTIO_NSM_RESPONSE_MAX) return MULIB_VMMDEV_EINVAL;
  return mulib_virtio_guest_transact(g, queue, input, input_size, output, capacity);
}
