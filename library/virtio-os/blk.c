/* Canonical implementation; regenerate single-header copies with tools/amalgamate_virtio.py. */
#include "internal.h"

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_block_(mulib_virtio_os_device_t *d, size_t ni, size_t no, size_t *used)
{
  uint32_t type;
  uint64_t sector, capacity;
  size_t n = 0, done = 0;
  unsigned op = 0;
  int rc;
  unsigned char status = 0;
  if (ni < 16 || no < 1 || mulib_endian_load_le32(d->input + 4)) return MULIB_VMMDEV_EINVAL;
  type = mulib_endian_load_le32(d->input); sector = mulib_vmmdev_load64(d->input + 8);
  capacity = mulib_vmmdev_load64(d->config);
  switch (type) {
  case 0:
    if (ni != 16 || (no - 1) % 512) return MULIB_VMMDEV_EINVAL;
    n = no - 1; op = MULIB_VIRTIO_SERVICE_READ; break;
  case 1:
    if (no != 1 || (ni - 16) % 512) return MULIB_VMMDEV_EINVAL;
    n = ni - 16; op = MULIB_VIRTIO_SERVICE_WRITE;
    if (d->offered & (UINT64_C(1) << 5)) status = 1;
    break;
  case 4:
    if (ni != 16 || no != 1 || sector) return MULIB_VMMDEV_EINVAL;
    if (!(d->negotiated & (UINT64_C(1) << 9))) status = 2;
    op = MULIB_VIRTIO_SERVICE_FLUSH; break;
  case 8:
    if (ni != 16 || no != 21 || sector) return MULIB_VMMDEV_EINVAL;
    op = MULIB_VIRTIO_SERVICE_ID; break;
  default: status = 2; break;
  }
  if ((type == 0 || type == 1) && (sector > capacity || n / 512 > capacity - sector)) status = 1;
  if (!status) {
    rc = mulib_virtio_os_call_(d, op, 0, sector * 512, 0,
                              d->input + 16, type == 1 ? n : 0,
                              d->output, type == 0 ? n : (type == 8 ? 20 : 0), &done);
    if (rc == MULIB_VMMDEV_AGAIN) return rc;
    if (rc || (type == 0 && done != n) || (type == 8 && done != 20)) status = 1;
  }
  if (status) memset(d->output, 0, no);
  d->output[no - 1] = status;
  *used = no; return MULIB_VMMDEV_OK;
}

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_blk_(const mulib_virtio_os_device_t *d)
{
  if (mulib_vmmdev_load64(d->config) > UINT64_MAX / 512) return MULIB_VMMDEV_EINVAL;
    return MULIB_VMMDEV_OK;
}
