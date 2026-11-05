/* Canonical implementation; regenerate single-header copies with tools/amalgamate_virtio.py. */
#include "internal.h"

int mulib_virtio_guest_gpio(mulib_virtio_guest_t *g, uint16_t type, uint16_t line, uint32_t *value)
{
  unsigned char request[8], response[2];
  size_t n = 2;
  int rc;
  if (!g || g->device_id != MULIB_VIRTIO_GPIO || !value || type < 2 || type > 5) return MULIB_VMMDEV_EINVAL;
  rc = mulib_virtio_gpio_request(request, type, line, type == 3 || type == 5 ? *value : 0);
  if (rc) return rc;
  rc = mulib_virtio_guest_transact(g, 0, request, 8, response, &n); if (rc) return rc;
  if (n != 2 || response[0] || (type == 2 && response[1] > 2) || (type == 4 && response[1] > 1))
    return MULIB_VMMDEV_EIO;
  if (type == 2 || type == 4) *value = response[1];
  return 0;
}

int mulib_virtio_guest_gpio_request(mulib_virtio_guest_t *g, unsigned queue,
    const void *input, size_t input_size, void *output, size_t *capacity)
{
  if (!g || g->device_id != MULIB_VIRTIO_GPIO) return MULIB_VMMDEV_EINVAL;
  return mulib_virtio_guest_transact(g, queue, input, input_size, output, capacity);
}
