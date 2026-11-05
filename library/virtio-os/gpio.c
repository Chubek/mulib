/* Canonical implementation; regenerate single-header copies with tools/amalgamate_virtio.py. */
#include "internal.h"

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_gpio_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used)
{
  unsigned char *in = d->input;
  unsigned char *out = d->output;
  size_t done = 0;
  int rc;
    uint16_t type, line;
    uint32_t value;
    unsigned char h[8];
    if (ni != 8 || no < 1) return MULIB_VMMDEV_EINVAL;
    type = mulib_vmmdev_load16(in); line = mulib_vmmdev_load16(in + 2);
    value = mulib_endian_load_le32(in + 4);
    if (type == 1) { out[0] = 1; *used = 1; return 0; } /* names_size is zero */
    if (no < 2) return MULIB_VMMDEV_EINVAL;
    rc = mulib_virtio_gpio_request(h, type, line, value);
    if (rc || type == 1 || line >= mulib_vmmdev_load16(d->config)) rc = MULIB_VMMDEV_EINVAL;
    else rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_GPIO, q, line, type,
                                   in + 4, 4, out + 1, 1, &done);
    if (rc == MULIB_VMMDEV_AGAIN) return rc;
    if (!rc && ((type == 2 && (done != 1 || out[1] > 2))
                || (type == 4 && (done != 1 || out[1] > 1)))) rc = MULIB_VMMDEV_EIO;
    if (rc || type == 3 || type == 5) out[1] = 0;
    out[0] = rc ? 1 : 0; done = 2; rc = 0;
  if (rc) return rc;
  *used = done; return MULIB_VMMDEV_OK;
}

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_gpio_(const mulib_virtio_os_device_t *d)
{
  if (!mulib_vmmdev_load16(d->config) || !mulib_virtio_os_zero_(d->config + 2, 6))
      return MULIB_VMMDEV_EINVAL;
    return MULIB_VMMDEV_OK;
}
