/* Canonical implementation; regenerate single-header copies with tools/amalgamate_virtio.py. */
#include "internal.h"

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_iommu_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used)
{
  unsigned char *in = d->input, *out = d->output;
  unsigned type, status = 0;
  size_t done;
  int rc;
  if (q == 1) {
    if (ni || no < 24) return MULIB_VMMDEV_EINVAL;
    rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_RECEIVE, q, 0, 0, NULL, 0, out, 24, &done);
    if (!rc && (done != 24 || out[0] > 2 || (mulib_endian_load_le32(out + 4) & ~UINT32_C(0x103))
                || !mulib_virtio_os_zero_(out + 1, 3) || mulib_endian_load_le32(out + 12)))
      return MULIB_VMMDEV_EIO;
    if (!rc) *used = done;
    return rc;
  }
  if (ni < 4 || no < 4) return MULIB_VMMDEV_EINVAL;
  type = in[0];
  if (type < 1 || type > 4) status = 2;
  else if ((type <= 2 && ni != 20) || (type == 3 && ni != 36) || (type == 4 && ni != 28)) status = 4;
  else if (type == 1 && mulib_endian_load_le32(in + 12)) status = 2;
  else if (type >= 3) {
    uint64_t start = mulib_vmmdev_load64(in + 8), end = mulib_vmmdev_load64(in + 16);
    if (start > end || (start & 4095) || (end & 4095) != 4095) status = 5;
    if (!status && type == 3) {
      uint64_t phys = mulib_vmmdev_load64(in + 24);
      uint32_t flags = mulib_endian_load_le32(in + 32);
      if ((phys & 4095) || end - start > UINT64_MAX - phys) status = 5;
      if (!flags || (flags & ~3u)) status = 2;
    }
  }
  if (!status) {
    rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_IOMMU, q, mulib_endian_load_le32(in + 4),
                              type, in + 4, ni - 4, out, 1, &done);
    if (rc == MULIB_VMMDEV_AGAIN) return rc;
    status = rc || done != 1 || out[0] > 8 ? 3 : out[0];
  }
  memset(out, 0, 4); out[0] = (unsigned char)status; *used = 4; return 0;
}

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_iommu_(const mulib_virtio_os_device_t *d)
{
  if (mulib_vmmdev_load64(d->config) != 4096) return MULIB_VMMDEV_EINVAL;
    return MULIB_VMMDEV_OK;
}
