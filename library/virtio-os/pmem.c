/* Canonical implementation; regenerate single-header copies with tools/amalgamate_virtio.py. */
#include "internal.h"

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_pmem_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used)
{
  unsigned char *in = d->input;
  unsigned char *out = d->output;
  size_t done = 0;
  int rc;
    if (ni != 4 || no < 4 || mulib_endian_load_le32(in)) return MULIB_VMMDEV_EINVAL;
    rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_FLUSH, q, 0, 0, NULL, 0, NULL, 0, &done);
    if (rc == MULIB_VMMDEV_AGAIN) return rc;
    mulib_endian_store_le32(out, rc ? 1 : 0); done = 4; rc = 0;
  if (rc) return rc;
  *used = done; return MULIB_VMMDEV_OK;
}

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_pmem_(const mulib_virtio_os_device_t *d)
{
  if (!mulib_vmmdev_load64(d->config + 8)
        || mulib_vmmdev_load64(d->config + 8) > UINT64_MAX - mulib_vmmdev_load64(d->config))
      return MULIB_VMMDEV_EINVAL;
    return MULIB_VMMDEV_OK;
}
