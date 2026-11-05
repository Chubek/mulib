/* Canonical implementation; regenerate single-header copies with tools/amalgamate_virtio.py. */
#include "internal.h"

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_rng_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used)
{
  unsigned char *out = d->output;
  size_t done = 0;
  int rc;
    if (ni || !no) return MULIB_VMMDEV_EINVAL;
    rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_ENTROPY, q, 0, 0, NULL, 0, out, no, &done);
    if (!rc && !done) return MULIB_VMMDEV_EIO;
  if (rc) return rc;
  *used = done; return MULIB_VMMDEV_OK;
}
