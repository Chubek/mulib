/* Canonical implementation; regenerate single-header copies with tools/amalgamate_virtio.py. */
#include "internal.h"

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_scsi_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used)
{
  unsigned char *in = d->input, *out = d->output;
  size_t done = 0;
  int rc;
  if (q == 1) return ni || no < 16 ? MULIB_VMMDEV_EINVAL : MULIB_VMMDEV_AGAIN;
  if (q == 0) {
    uint32_t type;
    if (ni < 4 || !no) return MULIB_VMMDEV_EINVAL;
    type = mulib_endian_load_le32(in);
    if (type == 0) {
      if (ni != 24 || no < 1) return MULIB_VMMDEV_EINVAL;
      if (mulib_endian_load_le32(in + 4) > 7) { out[0] = 11; *used = 1; return 0; }
      rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_SCSI_TMF, q,
                                mulib_vmmdev_load64(in + 16), mulib_endian_load_le32(in + 4),
                                in + 8, 8, out, 1, &done);
      if (rc == MULIB_VMMDEV_AGAIN) return rc;
      if (rc || done != 1 || out[0] > 12) out[0] = 9;
      *used = 1; return 0;
    }
    if ((type == 1 || type == 2) && ni == 16 && no >= 5) {
      memset(out, 0, 5); *used = 5; return 0; /* no asynchronous events */
    }
    return MULIB_VMMDEV_ENOTSUP;
  }
  {
    size_t cdb = mulib_endian_load_le32(d->config + 24), sense = mulib_endian_load_le32(d->config + 20);
    size_t header = 19 + cdb, result = 12 + sense;
    if (ni < header || no < result || in[16] > 3) return MULIB_VMMDEV_EINVAL;
    if (ni > header && no > result) { memset(out, 0, result); out[11] = 9; *used = result; return 0; }
    rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_SCSI_COMMAND, q,
                              mulib_vmmdev_load64(in + 8), (uint32_t)sense,
                              in, ni, out, no, &done);
    if (rc == MULIB_VMMDEV_AGAIN) return rc;
    if (rc || done < result || out[11] > 9 || mulib_endian_load_le32(out) > sense
        || mulib_endian_load_le32(out + 4) > (ni - header) + (no - result)) {
      memset(out, 0, result); out[11] = 9; done = result;
    }
  }
  *used = done; return 0;
}

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_scsi_(const mulib_virtio_os_device_t *d)
{
  if (mulib_endian_load_le32(d->config) != 1 || !mulib_endian_load_le32(d->config + 4)
        || mulib_endian_load_le32(d->config + 4) > MULIB_VMMDEV_MAX_SPANS - 2
        || !mulib_endian_load_le32(d->config + 8) || !mulib_endian_load_le32(d->config + 12)
        || mulib_endian_load_le32(d->config + 16) != 16
        || mulib_endian_load_le32(d->config + 20) != 96
        || mulib_endian_load_le32(d->config + 24) != 32 || mulib_vmmdev_load16(d->config + 28)
        || mulib_vmmdev_load16(d->config + 30) > 255 || mulib_endian_load_le32(d->config + 32) > 16383)
      return MULIB_VMMDEV_EINVAL;
    return MULIB_VMMDEV_OK;
}
