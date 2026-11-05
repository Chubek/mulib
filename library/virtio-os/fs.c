#include "internal.h"

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_fs_(mulib_virtio_os_device_t *d, unsigned q,
                                            size_t ni, size_t no, size_t *used)
{
  const unsigned char *in = d->input, *out = d->output;
  uint32_t opcode;
  int forget, rc;
  if (ni < 40 || mulib_endian_load_le32(in) != ni) return MULIB_VMMDEV_EINVAL;
  opcode = mulib_endian_load_le32(in + 4); forget = opcode == 2 || opcode == 42 || opcode == 41;
  if ((!q && opcode != 2 && opcode != 42) || (forget ? no != 0 : no < 16)) return MULIB_VMMDEV_EINVAL;
  rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_REQUEST, q, 0, 0,
                            in, ni, d->output, no, used);
  if (rc) return rc;
  if (no && (*used < 16 || mulib_endian_load_le32(out) != *used
      || mulib_vmmdev_load64(out + 8) != mulib_vmmdev_load64(in + 8))) return MULIB_VMMDEV_EIO;
  return 0;
}

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_fs_(const mulib_virtio_os_device_t *d)
{
  if (!d->config[0] || !memchr(d->config, 0, 36) || mulib_endian_load_le32(d->config + 36) != 1)
    return MULIB_VMMDEV_EINVAL;
  return 0;
}
