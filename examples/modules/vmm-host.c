#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_VMMDEV
#define MULIB_IMPL_VIRTIO
#define MULIB_IMPL_VMM_HOST
#include "mulib-vmm-host.h"
#include <assert.h>
#include <string.h>

int main(void)
{
  unsigned char media[512] = {0}, data[512], config[8];
  mulib_vmm_host_block_t disk;
  mulib_virtio_service_request_t request;
  uint64_t features;
  size_t used;
  assert(!mulib_vmm_host_block_init(&disk, media, sizeof media, 0, NULL));
  assert(!mulib_vmm_host_block_config(&disk, config, &features));
  assert(mulib_vmmdev_load64(config) == 1);
  memset(data, 0x45, sizeof data); memset(&request, 0, sizeof request);
  request.device_id = MULIB_VIRTIO_BLK; request.operation = MULIB_VIRTIO_SERVICE_WRITE;
  request.input = data; request.input_size = sizeof data;
  assert(!mulib_vmm_host_block_service(&disk, &request, &used));
  assert(!memcmp(media, data, sizeof data));
  return 0;
}
