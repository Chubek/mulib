/* Canonical implementation; regenerate single-header copies with tools/amalgamate_virtio.py. */
#include "internal.h"

int mulib_virtio_guest_i2c(mulib_virtio_guest_t *g, uint16_t address, uint32_t flags, void *bytes, size_t n)
{
  size_t ni, no, used;
  unsigned char h[8];
  int rc;
  if (!g || g->device_id != MULIB_VIRTIO_I2C || (!bytes && n) || n > MULIB_VIRTIO_TRANSFER_MAX - 8)
    return MULIB_VMMDEV_EINVAL;
  rc = mulib_virtio_i2c_header(h, address, flags); if (rc) return rc;
  ni = flags & 2 ? 8 : 8 + n; no = flags & 2 ? n + 1 : 1;
  rc = mulib_virtio_guest_workspace_(g, ni, no); if (rc) return rc;
  memcpy(g->scratch, h, 8);
  if (!(flags & 2) && n) memcpy(g->scratch + 8, bytes, n);
  rc = mulib_virtio_guest_prepared_(g, 0, ni, no, &used); if (rc) return rc;
  if (used != no || g->scratch[ni + no - 1]) return MULIB_VMMDEV_EIO;
  if ((flags & 2) && n) memcpy(bytes, g->scratch + ni, n);
  return 0;
}

int mulib_virtio_guest_i2c_request(mulib_virtio_guest_t *g, unsigned queue,
    const void *input, size_t input_size, void *output, size_t *capacity)
{
  if (!g || g->device_id != MULIB_VIRTIO_I2C) return MULIB_VMMDEV_EINVAL;
  return mulib_virtio_guest_transact(g, queue, input, input_size, output, capacity);
}
