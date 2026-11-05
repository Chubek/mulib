#include "internal.h"

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_input_(mulib_virtio_os_device_t *d, unsigned q,
                                               size_t ni, size_t no, size_t *used)
{
  int rc;
  if (!q) {
    if (ni || no < 8 || no % 8) return MULIB_VMMDEV_EINVAL;
    rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_RECEIVE, q, 0, 0,
                              NULL, 0, d->output, no, used);
    if (rc) return rc;
    if (!*used) return MULIB_VMMDEV_AGAIN;
    return *used % 8 ? MULIB_VMMDEV_EIO : 0;
  }
  if (no || !ni || ni % 8) return MULIB_VMMDEV_EINVAL;
  return mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_SEND, q, 0, 0,
                               d->input, ni, NULL, 0, used);
}

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_input_(const mulib_virtio_os_device_t *d)
{
  return mulib_virtio_os_zero_(d->config, d->config_size) ? 0 : MULIB_VMMDEV_EINVAL;
}
