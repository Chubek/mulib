#include "internal.h"

int mulib_virtio_guest_can_request(mulib_virtio_guest_t *g, unsigned queue,
    const void *input, size_t input_size, void *output, size_t *capacity)
{
  if (!g || g->device_id != MULIB_VIRTIO_CAN) return MULIB_VMMDEV_EINVAL;
  return mulib_virtio_guest_transact(g, queue, input, input_size, output, capacity);
}

int mulib_virtio_guest_can_control(mulib_virtio_guest_t *g, unsigned start)
{
  unsigned char input[2], output;
  size_t size = 1;
  int rc;
  if (start > 1) return MULIB_VMMDEV_EINVAL;
  mulib_vmmdev_store16(input, (uint16_t)(start ? 0x201 : 0x202));
  rc = mulib_virtio_guest_can_request(g, 2, input, 2, &output, &size);
  if (rc) return rc;
  return size == 1 && !output ? 0 : MULIB_VMMDEV_EIO;
}
