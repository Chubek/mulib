/* Canonical implementation; regenerate single-header copies with tools/amalgamate_virtio.py. */
#include "internal.h"

static int mulib_virtio_os_vsock_valid_(const unsigned char *p, size_t n, uint64_t cid, int receive)
{
  uint32_t len, flags;
  unsigned op;
  if (n < 44) return 0;
  len = mulib_endian_load_le32(p + 24); op = mulib_vmmdev_load16(p + 30);
  flags = mulib_endian_load_le32(p + 32);
  return len <= n - 44 && mulib_vmmdev_load64(p + (receive ? 8 : 0)) == cid
    && mulib_vmmdev_load64(p + (receive ? 0 : 8)) <= UINT32_MAX
    && op >= 1 && op <= 7 && (op == 5 || !len) && (op == 4 ? !(flags & ~3u) : !flags);
}
MULIB_VIRTIO_INTERNAL int mulib_virtio_os_vsock_(mulib_virtio_os_device_t *d, unsigned q, size_t ni, size_t no, size_t *used)
{
  size_t done = 0;
  int rc;
  uint64_t cid = mulib_vmmdev_load64(d->config);
  if (q == 1) {
    if (no || !mulib_virtio_os_vsock_valid_(d->input, ni, cid, 0)) return MULIB_VMMDEV_EINVAL;
    /* The socket service owns connections, credit accounting, and RST replies
       for unknown socket types; this adapter never silently drops such packets. */
    rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_SEND, q, cid, 0,
                              d->input, 44u + mulib_endian_load_le32(d->input + 24), NULL, 0, &done);
  } else {
    if (ni || no < (q == 2 ? 4u : 44u)) return MULIB_VMMDEV_EINVAL;
    rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_RECEIVE, q, cid, 0, NULL, 0,
                              d->output, q == 2 ? 4 : no, &done);
    if (!rc && ((q == 2 && (done != 4 || mulib_endian_load_le32(d->output)))
                || (q == 0 && (!mulib_virtio_os_vsock_valid_(d->output, done, cid, 1)
                              || done != 44u + mulib_endian_load_le32(d->output + 24)))))
      return MULIB_VMMDEV_EIO;
  }
  if (!rc) *used = done;
  return rc;
}

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_vsock_(const mulib_virtio_os_device_t *d)
{
  if (mulib_vmmdev_load64(d->config) < 3 || mulib_vmmdev_load64(d->config) >= UINT32_MAX)
      return MULIB_VMMDEV_EINVAL;
    return MULIB_VMMDEV_OK;
}
