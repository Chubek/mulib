#include "internal.h"

int mulib_virtio_guest_rtc_request(mulib_virtio_guest_t *g, unsigned queue,
    const void *input, size_t input_size, void *output, size_t *capacity)
{
  if (!g || g->device_id != MULIB_VIRTIO_RTC) return MULIB_VMMDEV_EINVAL;
  return mulib_virtio_guest_transact(g, queue, input, input_size, output, capacity);
}

int mulib_virtio_guest_rtc_read(mulib_virtio_guest_t *g, uint64_t *nanoseconds)
{
  unsigned char input[16] = {1}, output[16];
  size_t size = sizeof output;
  int rc;
  if (!nanoseconds) return MULIB_VMMDEV_EINVAL;
  rc = mulib_virtio_guest_rtc_request(g, 0, input, sizeof input, output, &size);
  if (rc) return rc;
  if (size != 16 || output[0]) return MULIB_VMMDEV_EIO;
  *nanoseconds = mulib_vmmdev_load64(output + 8); return 0;
}
