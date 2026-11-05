#include "internal.h"

static int mulib_virtio_os_rpmsg_frame_(const unsigned char *p, size_t size)
{
  return size >= 16 && mulib_vmmdev_load16(p + 12) == size - 16
    && mulib_virtio_os_zero_(p + 8, 4) && !mulib_vmmdev_load16(p + 14);
}

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_rpmsg_(mulib_virtio_os_device_t *d, unsigned q,
                                               size_t ni, size_t no, size_t *used)
{
  int rc;
  if (!q) {
    if (ni || no < 16) return MULIB_VMMDEV_EINVAL;
    rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_RECEIVE, q, 0, 0,
                              NULL, 0, d->output, no, used);
    if (rc) return rc;
    if (!*used) return MULIB_VMMDEV_AGAIN;
    return mulib_virtio_os_rpmsg_frame_(d->output, *used) ? 0 : MULIB_VMMDEV_EIO;
  }
  if (no || !mulib_virtio_os_rpmsg_frame_(d->input, ni)) return MULIB_VMMDEV_EINVAL;
  return mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_SEND, q, 0, 0,
                               d->input, ni, NULL, 0, used);
}
