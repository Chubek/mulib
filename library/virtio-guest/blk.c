/* Canonical implementation; regenerate single-header copies with tools/amalgamate_virtio.py. */
#include "internal.h"

int mulib_virtio_guest_block(mulib_virtio_guest_t *g, uint32_t type, uint64_t sector, void *data, size_t n)
{
  size_t ni, no, used;
  unsigned char status;
  int rc;
  if (!g || g->device_id != MULIB_VIRTIO_BLK || (!data && n)
      || ((type == 0 || type == 1) && n % 512) || (type == 4 && (n || sector))
      || (type == 8 && (n != 20 || sector)) || (type != 0 && type != 1 && type != 4 && type != 8)
      || n > MULIB_VIRTIO_TRANSFER_MAX - 16) return MULIB_VMMDEV_EINVAL;
  ni = type == 1 ? 16 + n : 16; no = type == 0 || type == 8 ? n + 1 : 1;
  rc = mulib_virtio_guest_workspace_(g, ni, no); if (rc) return rc;
  mulib_virtio_blk_header(g->scratch, type, sector);
  if (type == 1 && n) memcpy(g->scratch + 16, data, n);
  rc = mulib_virtio_guest_prepared_(g, 0, ni, no, &used); if (rc) return rc;
  if (used != no) return MULIB_VMMDEV_EIO;
  status = g->scratch[ni + no - 1];
  if (status) return status == 2 ? MULIB_VMMDEV_ENOTSUP : MULIB_VMMDEV_EIO;
  if ((type == 0 || type == 8) && n) memcpy(data, g->scratch + ni, n);
  return 0;
}

int mulib_virtio_guest_blk_request(mulib_virtio_guest_t *g, unsigned queue,
    const void *input, size_t input_size, void *output, size_t *capacity)
{
  if (!g || g->device_id != MULIB_VIRTIO_BLK) return MULIB_VMMDEV_EINVAL;
  return mulib_virtio_guest_transact(g, queue, input, input_size, output, capacity);
}
