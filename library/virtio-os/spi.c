/* Virtio SPI wire validation. The provider owns controller and chip-select state. */
#include "internal.h"

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_spi_(const mulib_virtio_os_device_t *d)
{
  uint32_t modes = mulib_endian_load_le32(d->config + 8);
  if (!d->config[0] || d->config[1] > 1 || (d->config[2] & ~7u) || (d->config[3] & ~7u)
      || (modes & ~127u) || !(modes & 3) || !(modes & 12)) return MULIB_VMMDEV_EINVAL;
  return 0;
}

static int mulib_virtio_os_spi_width_(unsigned width, unsigned supported)
{
  if (width <= 1) return 1;
  if (width == 2) return (supported & 1) != 0;
  if (width == 4) return (supported & 2) != 0;
  if (width == 8) return (supported & 4) != 0;
  return 0;
}

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_spi_(mulib_virtio_os_device_t *d, unsigned q,
                                             size_t ni, size_t no, size_t *used)
{
  const unsigned char *p = d->input, *c = d->config;
  uint32_t mode, modes, words;
  unsigned bits, unit;
  size_t tx, rx, done;
  int rc = MULIB_VMMDEV_EINVAL;
  *used = 0;
  if (q || ni < 32 || !no) return MULIB_VMMDEV_EINVAL;
  tx = ni - 32; rx = no - 1; bits = p[1];
  mode = mulib_endian_load_le32(p + 8); modes = mulib_endian_load_le32(c + 8);
  words = mulib_endian_load_le32(c + 4);
  unit = bits <= 8 ? 1 : bits <= 16 ? 2 : 4;
  if (p[0] >= c[0] || !bits || bits > 32 || (words && !(words & (UINT32_C(1) << (bits - 1))))
      || p[2] > 1 || (p[2] && !c[1]) || !mulib_virtio_os_zero_(p + 5, 3)
      || !mulib_virtio_os_spi_width_(p[3], c[2]) || !mulib_virtio_os_spi_width_(p[4], c[3])
      || (mode & ~31u) || !(modes & (1u << (mode & 1)))
      || !(modes & (4u << ((mode >> 1) & 1)))
      || ((mode & 4) && !(modes & 16)) || ((mode & 8) && !(modes & 32))
      || ((mode & 16) && !(modes & 64)) || tx % unit || rx % unit || (tx && rx && tx != rx))
    goto complete;
  /* Frequency zero is invalid; a zero configured maximum means unlimited. */
  if (!mulib_endian_load_le32(p + 12)
      || (mulib_endian_load_le32(c + 12)
          && mulib_endian_load_le32(p + 12) > mulib_endian_load_le32(c + 12))) goto complete;
  for (unsigned i = 16; i < 32; i += 4)
    if (mulib_endian_load_le32(p + i) > mulib_endian_load_le32(c + i)) goto complete;
  rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_SPI, q, p[0], mode, p, ni,
                            d->output, rx, &done);
  if (rc == MULIB_VMMDEV_AGAIN) return rc;
  if (!rc && done != rx) rc = MULIB_VMMDEV_EIO;
complete:
  if (rc) memset(d->output, 0, rx);
  d->output[rx] = rc == MULIB_VMMDEV_EINVAL ? 1 : rc ? 2 : 0;
  *used = no; return 0;
}
