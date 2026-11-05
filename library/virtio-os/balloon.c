/* Canonical implementation; regenerate single-header copies with tools/amalgamate_virtio.py. */
#include "internal.h"

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_balloon_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used)
{
  unsigned char *in = d->input;
  unsigned op;
  size_t done = 0;
  int rc;
    if (!ni || ni % 4 || no) return MULIB_VMMDEV_EINVAL;
    op = q == 0 ? MULIB_VIRTIO_SERVICE_INFLATE : MULIB_VIRTIO_SERVICE_DEFLATE;
    rc = mulib_virtio_os_call_(d, op, q, 0, 0, in, ni, NULL, 0, &done);
  if (rc) return rc;
  *used = done; return MULIB_VMMDEV_OK;
}

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_balloon_(const mulib_virtio_os_device_t *d)
{
  if (mulib_endian_load_le32(d->config + 4)) return MULIB_VMMDEV_EINVAL;
    return MULIB_VMMDEV_OK;
}
