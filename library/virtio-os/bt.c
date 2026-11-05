/* Virtio Bluetooth primary controller, packed v1 configuration, no vendor extensions. */
#include "internal.h"

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_validate_bt_(const mulib_virtio_os_device_t *d)
{
  return mulib_virtio_os_zero_(d->config, 5) ? 0 : MULIB_VMMDEV_EINVAL;
}

static int mulib_virtio_os_bt_packet_(const unsigned char *p, size_t n, unsigned receive)
{
  size_t header, payload;
  if (!n) return 0;
  switch (p[0]) {
  case 1: /* HCI command */
    if (receive || n < 4) return 0;
    header = 4; payload = p[3]; break;
  case 2: /* ACL */
    if (n < 5) return 0;
    header = 5; payload = mulib_vmmdev_load16(p + 3); break;
  case 3: /* SCO */
    if (n < 4) return 0;
    header = 4; payload = p[3]; break;
  case 4: /* HCI event */
    if (!receive || n < 3) return 0;
    header = 3; payload = p[2]; break;
  case 5: /* ISO */
    if (n < 5 || (p[4] & 0xc0)) return 0;
    header = 5; payload = mulib_vmmdev_load16(p + 3); break;
  default: return 0;
  }
  return n == header + payload;
}

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_bt_(mulib_virtio_os_device_t *d, unsigned q,
                                            size_t ni, size_t no, size_t *used)
{
  size_t done;
  int rc;
  *used = 0;
  if (!q) {
    if (no || !mulib_virtio_os_bt_packet_(d->input, ni, 0)) return MULIB_VMMDEV_EINVAL;
    return mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_SEND, q, 0, 0,
                                 d->input, ni, NULL, 0, &done);
  }
  if (q != 1 || ni || !no) return MULIB_VMMDEV_EINVAL;
  rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_RECEIVE, q, 0, 0,
                            NULL, 0, d->output, no, &done);
  if (rc) return rc;
  if (!done) return MULIB_VMMDEV_AGAIN;
  if (!mulib_virtio_os_bt_packet_(d->output, done, 1)) return MULIB_VMMDEV_EIO;
  *used = done; return 0;
}
