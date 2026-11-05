#include "internal.h"

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_console_(mulib_virtio_os_device_t *d, unsigned q,
                                                 size_t ni, size_t no, size_t *used)
{
  int rc;
  if (!q) {
    if (ni || !no) return MULIB_VMMDEV_EINVAL;
    rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_RECEIVE, q, 0, 0,
                              NULL, 0, d->output, no, used);
    return !rc && !*used ? MULIB_VMMDEV_AGAIN : rc;
  }
  if (no || !ni) return MULIB_VMMDEV_EINVAL;
  return mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_SEND, q, 0, 0,
                               d->input, ni, NULL, 0, used);
}
