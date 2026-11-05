#include "internal.h"

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_p9_(mulib_virtio_os_device_t *d, unsigned q,
                                            size_t ni, size_t no, size_t *used)
{
  const unsigned char *in = d->input, *out = d->output;
  int rc;
  if (ni < 7 || no < 7 || mulib_endian_load_le32(in) != ni) return MULIB_VMMDEV_EINVAL;
  rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_REQUEST, q, 0, 0,
                            in, ni, d->output, no, used);
  if (rc) return rc;
  if (*used < 7 || mulib_endian_load_le32(out) != *used
      || mulib_vmmdev_load16(out + 5) != mulib_vmmdev_load16(in + 5)
      || (out[4] != (unsigned)in[4] + 1 && out[4] != 107 && out[4] != 7)) return MULIB_VMMDEV_EIO;
  return 0;
}

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_p9_(const mulib_virtio_os_device_t *d)
{
  if (d->config_size < 3 || mulib_vmmdev_load16(d->config) != d->config_size - 2)
    return MULIB_VMMDEV_EINVAL;
  return 0;
}
