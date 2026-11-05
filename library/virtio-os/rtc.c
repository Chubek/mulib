/* Virtio RTC baseline: one monotonic nanosecond clock, no alarms/cross timestamps. */
#include "internal.h"

MULIB_VIRTIO_INTERNAL int mulib_virtio_os_rtc_(mulib_virtio_os_device_t *d, unsigned q,
                                             size_t ni, size_t no, size_t *used)
{
  unsigned type = 0, status = 0;
  size_t request = 16, response = 16, done;
  int rc;
  *used = 0;
  if (q || !no) return MULIB_VMMDEV_EINVAL;
  memset(d->output, 0, no < 24 ? no : 24);
  if (ni < 8 || no < 8) { status = 4; goto complete; }
  type = mulib_vmmdev_load16(d->input);
  if (!mulib_virtio_os_zero_(d->input + 2, 6)) { status = 4; goto complete; }
  switch (type) {
  case 0x1000: request = 8; break;
  case 1: case 0x1001: case 0x1002: break;
  case 2: response = 24; break;
  case 0x1003: response = 24; break;
  case 0x1004: request = 24; response = 8; break;
  case 0x1005: response = 8; break;
  default: status = 2; goto complete;
  }
  if (ni < request || no < response) { status = 4; goto complete; }
  if (type >= 0x1003 && type <= 0x1005) { status = 3; goto complete; }
  if (type != 0x1000) {
    unsigned counter = type == 2 || type == 0x1002;
    if (!mulib_virtio_os_zero_(d->input + (counter ? 11 : 10), counter ? 5 : 6)) {
      status = 4; goto complete;
    }
    if (mulib_vmmdev_load16(d->input + 8)) { status = 3; goto complete; }
    if (counter && d->input[10] > 1) { status = 2; goto complete; }
  }
  switch (type) {
  case 0x1000: mulib_vmmdev_store16(d->output + 8, 1); break;
  case 0x1001: d->output[8] = 2; break; /* monotonic, no alarm */
  case 0x1002: break; /* cross timestamping unavailable */
  case 2: status = 2; break;
  case 1:
    rc = mulib_virtio_os_call_(d, MULIB_VIRTIO_SERVICE_READ, q, 0, 1,
                              NULL, 0, d->output + 8, 8, &done);
    if (rc == MULIB_VMMDEV_AGAIN) return rc;
    if (rc || done != 8 || mulib_vmmdev_load64(d->output + 8) < d->rtc_reading) {
      status = 5; memset(d->output + 8, 0, 8);
    } else d->rtc_reading = mulib_vmmdev_load64(d->output + 8);
    break;
  default: break;
  }
complete:
  d->output[0] = (unsigned char)status;
  *used = status ? (no < 8 ? no : 8) : response;
  return 0;
}
