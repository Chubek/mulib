#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_VMMDEV
#define MULIB_IMPL_VIRTIO
#define MULIB_IMPL_VIRTIO_OS
#include "mulib-virtio-os.h"
#include <assert.h>
#include <string.h>

static int console(void *context, const mulib_virtio_service_request_t *r, size_t *used)
{
  (void)context; *used = 0;
  if (r->operation == MULIB_VIRTIO_SERVICE_RESET) return 0;
  if (r->operation == MULIB_VIRTIO_SERVICE_SEND)
    return fwrite(r->input, 1, r->input_size, stdout) == r->input_size ? 0 : MULIB_VMMDEV_EIO;
  return MULIB_VMMDEV_AGAIN;
}
int main(void)
{
  mulib_virtio_os_device_t device, *devices[] = {&device};
  mulib_virtio_os_options_t options;
  mulib_virtio_os_bus_t bus;
  mulib_vmmdev_memory_t memory = {NULL, 0};
  mulib_vmmdev_message_t message = {0}, reply;
  unsigned char input[512], output[512], request[40], response[72];
  size_t ns = sizeof request, nr = sizeof response;
  memset(&options, 0, sizeof options); options.instance = 1; options.device_id = MULIB_VIRTIO_CONSOLE;
  options.transfer_limit = sizeof input; options.input_scratch = input; options.output_scratch = output;
  options.service = console;
  assert(!mulib_virtio_os_init(&device, &options));
  assert(!mulib_virtio_os_bus_init(&bus, devices, 1, memory));
  message.opcode = MULIB_VMMDEV_PROBE; message.device = 1;
  assert(!mulib_vmmdev_encode(&message, request, &ns));
  assert(!mulib_virtio_os_exchange(&bus, request, ns, response, &nr));
  assert(!mulib_vmmdev_decode(response, nr, &reply));
  assert(!reply.argument && mulib_endian_load_le32(reply.payload) == MULIB_VIRTIO_CONSOLE);
  return 0;
}
