#include "internal.h"

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_scmi_(mulib_virtio_os_device_t *d, unsigned q,
                                              size_t ni, size_t no, size_t *used)
{
  const unsigned char *in = d->input, *out = d->output;
  int rc;
  if (ni < 4 || ni % 4 || no < 8 || (mulib_endian_load_le32(in) & UINT32_C(0xf0000300)))
    return MULIB_VMMDEV_EINVAL;
  rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_REQUEST, q, 0, 0,
                            in, ni, d->output, no, used);
  if (rc) return rc;
  if (*used < 8 || *used % 4 || mulib_endian_load_le32(out) != mulib_endian_load_le32(in)
      || (mulib_endian_load_le32(out + 4) && *used != 8)) return MULIB_VMMDEV_EIO;
  return 0;
}
