#include "internal.h"

int mulib_virtio_guest_spi_request(mulib_virtio_guest_t *g, unsigned queue,
    const void *input, size_t input_size, void *output, size_t *capacity)
{
  if (!g || g->device_id != MULIB_VIRTIO_SPI) return MULIB_VMMDEV_EINVAL;
  return mulib_virtio_guest_transact(g, queue, input, input_size, output, capacity);
}

int mulib_virtio_guest_spi_transfer(mulib_virtio_guest_t *g, const unsigned char header[32],
    const void *tx, size_t tx_size, void *rx, size_t rx_size)
{
  size_t ni, no, used;
  unsigned status;
  int rc;
  if (!g || g->device_id != MULIB_VIRTIO_SPI || !header || (!tx && tx_size) || (!rx && rx_size)
      || tx_size > MULIB_VIRTIO_TRANSFER_MAX - 32 || rx_size >= MULIB_VIRTIO_TRANSFER_MAX)
    return MULIB_VMMDEV_EINVAL;
  ni = 32 + tx_size; no = rx_size + 1;
  rc = mulib_virtio_guest_workspace_(g, ni, no); if (rc) return rc;
  memcpy(g->scratch, header, 32);
  if (tx_size) memcpy(g->scratch + 32, tx, tx_size);
  rc = mulib_virtio_guest_prepared_(g, 0, ni, no, &used); if (rc) return rc;
  if (used != no) return MULIB_VMMDEV_EIO;
  status = g->scratch[ni + rx_size];
  if (status) return status == 1 ? MULIB_VMMDEV_EINVAL : MULIB_VMMDEV_EIO;
  if (rx_size) memcpy(rx, g->scratch + ni, rx_size);
  return 0;
}
