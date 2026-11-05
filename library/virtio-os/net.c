#include "internal.h"

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_net_(mulib_virtio_os_device_t *d, unsigned q,
                                             size_t ni, size_t no, size_t *used)
{
  size_t done;
  int rc;
  if (!q) {
    if (ni || no <= 12) return MULIB_VMMDEV_EINVAL;
    memset(d->output, 0, 12);
    rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_RECEIVE, q, 0, 0,
                              NULL, 0, d->output + 12, no - 12, &done);
    if (rc) return rc;
    if (!done) return MULIB_VMMDEV_AGAIN;
    *used = done + 12; return 0;
  }
  if (no || ni <= 12 || !mulib_virtio_os_zero_(d->input, 12)) return MULIB_VMMDEV_EINVAL;
  return mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_SEND, q, 0, 0,
                               d->input + 12, ni - 12, NULL, 0, used);
}
