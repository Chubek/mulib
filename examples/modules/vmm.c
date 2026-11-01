#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_VMMDEV
#define MULIB_IMPL_VIRTIO
#define MULIB_IMPL_VIRTIO_OS
#define MULIB_IMPL_VIRTIO_GUEST
#define MULIB_IMPL_VMM_MEMORY
#define MULIB_IMPL_VMM
#define MULIB_IMPL_VMM_HOST
#include "mulib-vmm.h"
#include "mulib-vmm-host.h"
#include "mulib-virtio-guest.h"
#include <assert.h>
#include <string.h>

int main(void)
{
  mulib_vmm_t vm = MULIB_VMM_INIT;
  mulib_vmm_options_t options;
  mulib_virtio_os_options_t device_options;
  mulib_virtio_os_device_t device;
  mulib_virtio_guest_t guest;
  mulib_vmm_host_block_t disk;
  unsigned char ram[2048], media[1024] = {0}, input[1024], output[1024], config[8], data[512];
  mulib_vmmdev_region_t region = {0x1000, sizeof ram, ram, 3};
  mulib_vmm_binding_t binding;
  memset(&options, 0, sizeof options); memset(&device_options, 0, sizeof device_options);
  assert(!mulib_vmm_host_block_init(&disk, media, sizeof media, 0, NULL));
  assert(!mulib_vmm_host_block_config(&disk, config, &device_options.features));
  device_options.instance = 1; device_options.device_id = MULIB_VIRTIO_BLK;
  device_options.config = config; device_options.config_size = sizeof config;
  device_options.transfer_limit = sizeof input; device_options.input_scratch = input; device_options.output_scratch = output;
  device_options.service = mulib_vmm_host_block_service; device_options.context = &disk;
  assert(!mulib_virtio_os_init(&device, &device_options));
  binding.device = &device; binding.dma.regions = &region; binding.dma.count = 1;
  options.ram.regions = &region; options.ram.count = 1; options.devices = &binding; options.device_count = 1;
  assert(!mulib_vmm_init(&vm, &options));
  assert(!mulib_virtio_guest_attach(&guest, mulib_vmm_exchange, &vm, 1, UINT64_C(1) << 9, 0x1000, ram, sizeof ram));
  assert(!mulib_virtio_guest_start(&guest));
  memset(data, 0x6d, sizeof data); assert(!mulib_virtio_guest_block(&guest, 1, 1, data, sizeof data));
  memset(data, 0, sizeof data); assert(!mulib_virtio_guest_block(&guest, 0, 1, data, sizeof data));
  assert(data[0] == 0x6d && data[511] == 0x6d);
  assert(!mulib_vmm_close(&vm));
  return 0;
}
