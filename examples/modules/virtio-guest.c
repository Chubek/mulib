#ifndef MULIB_VIRTIO_EXAMPLE_COMPILED
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_VMMDEV
#define MULIB_IMPL_VIRTIO
#define MULIB_IMPL_VIRTIO_OS
#define MULIB_IMPL_VIRTIO_GUEST
#endif
#include "mulib-virtio-os.h"
#include "mulib-virtio-guest.h"
#include <assert.h>
#include <string.h>

static int storage(void *context, const mulib_virtio_service_request_t *r, size_t *used)
{
  unsigned char *disk = (unsigned char *)context;
  *used = 0;
  if (r->operation == MULIB_VIRTIO_SERVICE_RESET || r->operation == MULIB_VIRTIO_SERVICE_FLUSH) return 0;
  if (r->operation == MULIB_VIRTIO_SERVICE_READ) {
    memcpy(r->output, disk + r->address, r->output_capacity); *used = r->output_capacity; return 0;
  }
  if (r->operation == MULIB_VIRTIO_SERVICE_WRITE) {
    memcpy(disk + r->address, r->input, r->input_size); return 0;
  }
  if (r->operation == MULIB_VIRTIO_SERVICE_ID) {
    memset(r->output, 0, 20); memcpy(r->output, "mulib-memory-disk", 17); *used = 20; return 0;
  }
  return MULIB_VMMDEV_ENOTSUP;
}
int main(void)
{
  unsigned char disk[1024] = {0}, ram[2048], input[1024], output[1024], config[8];
  unsigned char written[512], readback[512];
  mulib_virtio_os_options_t options;
  mulib_virtio_os_device_t device, *devices[] = {&device};
  mulib_virtio_os_bus_t bus;
  mulib_virtio_guest_t guest;
  mulib_vmmdev_region_t region = {0x1000, sizeof ram, ram, 3};
  mulib_vmmdev_memory_t memory = {&region, 1};
  memset(&options, 0, sizeof options); mulib_vmmdev_store64(config, 2);
  options.instance = 1; options.device_id = MULIB_VIRTIO_BLK; options.features = UINT64_C(1) << 9;
  options.config = config; options.config_size = sizeof config; options.transfer_limit = sizeof input;
  options.input_scratch = input; options.output_scratch = output; options.service = storage; options.context = disk;
  assert(!mulib_virtio_os_init(&device, &options));
  assert(!mulib_virtio_os_bus_init(&bus, devices, 1, memory));
  assert(!mulib_virtio_guest_attach(&guest, mulib_virtio_os_exchange, &bus, 1, UINT64_C(1) << 9,
                                   0x1000, ram, sizeof ram));
  assert(!mulib_virtio_guest_start(&guest));
  memset(written, 0x37, sizeof written);
  assert(!mulib_virtio_guest_block(&guest, 1, 1, written, sizeof written));
  assert(!mulib_virtio_guest_block(&guest, 0, 1, readback, sizeof readback));
  assert(!memcmp(written, readback, sizeof written));
  assert(!mulib_virtio_guest_block(&guest, 4, 0, NULL, 0));
  assert(!mulib_virtio_guest_reset(&guest));
  return 0;
}
