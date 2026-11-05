/* Canonical implementation; regenerate single-header copies with tools/amalgamate_virtio.py. */
#include "internal.h"

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_i2c_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used)
{
  unsigned char *in = d->input;
  unsigned char *out = d->output;
  size_t done = 0;
  int rc;
    uint16_t address;
    uint32_t flags;
    unsigned char h[8];
    size_t n;
    if (ni < 8 || no < 1) return MULIB_VMMDEV_EINVAL;
    address = mulib_vmmdev_load16(in); flags = mulib_endian_load_le32(in + 4);
    if (mulib_virtio_i2c_header(h, address, flags) || mulib_vmmdev_load16(in + 2))
      return MULIB_VMMDEV_EINVAL;
    if ((flags & 2) ? ni != 8 : no != 1) return MULIB_VMMDEV_EINVAL;
    n = (flags & 2) ? no - 1 : ni - 8;
    rc = d->i2c_failed ? MULIB_VMMDEV_EIO :
      mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_I2C, q, address, flags,
                            in + 8, (flags & 2) ? 0 : n, out, (flags & 2) ? n : 0, &done);
    if (rc == MULIB_VMMDEV_AGAIN) return rc;
    if (!rc && (flags & 2) && done != n) rc = MULIB_VMMDEV_EIO;
    d->i2c_failed = rc && (flags & 1);
    if (rc) memset(out, 0, no);
    out[no - 1] = rc ? 1 : 0; done = no; rc = 0;
  if (rc) return rc;
  *used = done; return MULIB_VMMDEV_OK;
}
