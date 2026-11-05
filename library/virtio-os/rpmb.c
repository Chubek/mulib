/* Canonical implementation; regenerate single-header copies with tools/amalgamate_virtio.py. */
#include "internal.h"

static unsigned mulib_virtio_os_be16_(const unsigned char *p)
{ return (unsigned)p[0] << 8 | p[1]; }
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_rpmb_(mulib_virtio_os_device_t *d, size_t ni, size_t no, size_t *used)
{
  unsigned char *in = d->input, *out = d->output;
  unsigned type, blocks, result_read = 0;
  size_t frames, expected = 0, done;
  int rc;
  if (!ni || ni % 512 || no % 512) return MULIB_VMMDEV_EINVAL;
  frames = ni / 512; type = mulib_virtio_os_be16_(in + 510); blocks = mulib_virtio_os_be16_(in + 506);
  if (type == 1 || type == 3) {
    if (frames > 1 && mulib_virtio_os_be16_(in + ni - 2) == 5) { result_read = 1; --frames; expected = 512; }
    if (!blocks || (type == 1 && (blocks != 1 || frames != 1))
        || (type == 3 && (frames != blocks || (d->config[1] && blocks > d->config[1]))))
      return MULIB_VMMDEV_EINVAL;
    for (size_t i = 0; i < frames; ++i)
      if (mulib_virtio_os_be16_(in + i * 512 + 510) != type) return MULIB_VMMDEV_EINVAL;
    if (result_read && mulib_virtio_os_be16_(in + ni - 6) != 1) return MULIB_VMMDEV_EINVAL;
  } else if (type == 2 || type == 4 || type == 5) {
    if (frames != 1 || !blocks || ((type == 2 || type == 5) && blocks != 1)
        || (type == 4 && d->config[2] && blocks > d->config[2])) return MULIB_VMMDEV_EINVAL;
    expected = type == 4 ? (size_t)blocks * 512 : 512;
  } else return MULIB_VMMDEV_ENOTSUP;
  if (no != expected) return MULIB_VMMDEV_EINVAL;
  rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_RPMB, 0, 0, type, in, ni, out, no, &done);
  if (rc) return rc;
  if (done != expected) return MULIB_VMMDEV_EIO;
  for (size_t i = 0; i < done; i += 512) {
    unsigned response = mulib_virtio_os_be16_(out + i + 510);
    if ((type != 5 && response != type * 256) || (type == 5 && response != 256 && response != 768))
      return MULIB_VMMDEV_EIO;
  }
  *used = done; return 0;
}

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_rpmb_(const mulib_virtio_os_device_t *d)
{
  if (d->config[0] > 128) return MULIB_VMMDEV_EINVAL;
    return MULIB_VMMDEV_OK;
}
