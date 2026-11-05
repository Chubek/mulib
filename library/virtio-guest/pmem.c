/* Canonical implementation; regenerate single-header copies with tools/amalgamate_virtio.py. */
#include "internal.h"

int mulib_virtio_guest_pmem_flush(mulib_virtio_guest_t *g)
{
  unsigned char req[4] = {0}, rep[4];
  size_t n = 4;
  int rc;
  if (!g || g->device_id != MULIB_VIRTIO_PMEM) return MULIB_VMMDEV_EINVAL;
  rc = mulib_virtio_guest_transact(g, 0, req, 4, rep, &n); if (rc) return rc;
  return n == 4 && !mulib_endian_load_le32(rep) ? 0 : MULIB_VMMDEV_EIO;
}

int mulib_virtio_guest_pmem_request(mulib_virtio_guest_t *g, unsigned queue,
    const void *input, size_t input_size, void *output, size_t *capacity)
{
  if (!g || g->device_id != MULIB_VIRTIO_PMEM) return MULIB_VMMDEV_EINVAL;
  return mulib_virtio_guest_transact(g, queue, input, input_size, output, capacity);
}
