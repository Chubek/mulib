/* Canonical implementation; regenerate single-header copies with tools/amalgamate_virtio.py. */
#include "internal.h"

int mulib_virtio_guest_entropy(mulib_virtio_guest_t *g, void *out, size_t *n)
{
  if (!g || g->device_id != MULIB_VIRTIO_RNG) return MULIB_VMMDEV_EINVAL;
  return mulib_virtio_guest_transact(g, 0, NULL, 0, out, n);
}

int mulib_virtio_guest_rng_request(mulib_virtio_guest_t *g, unsigned queue,
    const void *input, size_t input_size, void *output, size_t *capacity)
{
  if (!g || g->device_id != MULIB_VIRTIO_RNG) return MULIB_VMMDEV_EINVAL;
  return mulib_virtio_guest_transact(g, queue, input, input_size, output, capacity);
}
