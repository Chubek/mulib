/* Virtio CAN: classic frames mandatory, FD and RTR optional; no late TX ACK. */
#include "internal.h"

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_can_(const mulib_virtio_os_device_t *d)
{
  return mulib_vmmdev_load16(d->config) ? MULIB_VMMDEV_EINVAL : 0;
}

static int mulib_virtio_os_can_frame_(const unsigned char *p, size_t n,
                                     uint64_t features, unsigned type)
{
  uint32_t flags, id;
  unsigned length;
  if (n < 16 || mulib_vmmdev_load16(p) != type) return 0;
  length = mulib_vmmdev_load16(p + 2);
  flags = mulib_endian_load_le32(p + 8); id = mulib_endian_load_le32(p + 12);
  if (flags & ~UINT32_C(0xe000)) return 0;
  if (id > ((flags & 0x8000) ? UINT32_C(0x1fffffff) : UINT32_C(0x7ff))) return 0;
  if ((flags & 0x4000) ? (!(features & 2) || length > 64) : (!(features & 1) || length > 8))
    return 0;
  if ((flags & 0x2000) && (!(features & 4) || (flags & 0x4000))) return 0;
  return n == 16u + length;
}

int mulib_virtio_os_can_busoff(mulib_virtio_os_device_t *d, unsigned busoff)
{
  if (!d || !d->profile || d->profile->id != MULIB_VIRTIO_CAN || busoff > 1)
    return MULIB_VMMDEV_EINVAL;
  if (!d->epoch) return MULIB_VMMDEV_ESTATE;
  if (mulib_vmmdev_load16(d->config) != busoff) {
    mulib_vmmdev_store16(d->config, (uint16_t)busoff); ++d->generation;
  }
  if (busoff) d->can_started = 0;
  return 0;
}

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_can_(mulib_virtio_os_device_t *d, unsigned q,
                                             size_t ni, size_t no, size_t *used)
{
  size_t done;
  unsigned type;
  int rc;
  *used = 0;
  if (q == 1) {
    if (ni || no < 16) return MULIB_VMMDEV_EINVAL;
    if (!d->can_started) return MULIB_VMMDEV_AGAIN;
    rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_RECEIVE, q, 0, 0,
                              NULL, 0, d->output, no, &done);
    if (rc) return rc;
    if (!done) return MULIB_VMMDEV_AGAIN;
    if (!mulib_virtio_os_can_frame_(d->output, done, d->negotiated, 0x101)) return MULIB_VMMDEV_EIO;
    *used = done; return 0;
  }
  if (q > 2 || no < 1) return MULIB_VMMDEV_EINVAL;
  if (q == 2) {
    if (ni != 2) return MULIB_VMMDEV_EINVAL;
    type = mulib_vmmdev_load16(d->input);
    if (type != 0x201 && type != 0x202) return MULIB_VMMDEV_EINVAL;
    rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_CAN_CONTROL, q, 0, type,
                              NULL, 0, NULL, 0, &done);
    if (!rc) {
      d->can_started = type == 0x201;
      if (d->can_started) (void)mulib_virtio_os_can_busoff(d, 0);
    }
  } else {
    if (!mulib_virtio_os_can_frame_(d->input, ni, d->negotiated, 1)
        || !mulib_virtio_os_zero_(d->input + 4, 4)) return MULIB_VMMDEV_EINVAL;
    rc = d->can_started ? mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_SEND, q, 0, 0,
                                              d->input, ni, NULL, 0, &done) : MULIB_VMMDEV_ESTATE;
  }
  if (rc == MULIB_VMMDEV_AGAIN) return rc;
  d->output[0] = rc ? 1 : 0; *used = 1; return 0;
}
