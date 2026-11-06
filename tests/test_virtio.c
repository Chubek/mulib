#ifndef MULIB_VIRTIO_TEST_COMPILED
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_VMMDEV
#define MULIB_IMPL_VIRTIO
#define MULIB_IMPL_VIRTIO_OS
#define MULIB_IMPL_VIRTIO_GUEST
#endif
#include "mulib-virtio-os.h"
#include "mulib-virtio-guest.h"
#include "virtio.h"
#include <assert.h>
#include <string.h>

typedef struct fixture {
  unsigned char ram[8192], input[4096], output[4096], disk[4096], packet[512];
  size_t packet_size;
  unsigned calls, fail, again, direction, value;
  unsigned short_reply;
  unsigned reset_result;
  uint64_t clock_ns;
  mulib_vmmdev_region_t region;
  mulib_virtio_os_device_t device;
  mulib_virtio_os_device_t *devices[1];
  mulib_virtio_os_bus_t bus;
  mulib_virtio_guest_t guest;
  unsigned corrupt_reply;
} fixture_t;

static int service(void *context, const mulib_virtio_service_request_t *r, size_t *used)
{
  fixture_t *f = (fixture_t *)context;
  ++f->calls; *used = 0;
  if (r->operation == MULIB_VIRTIO_SERVICE_RESET) return (int)f->reset_result;
  if (f->again) return MULIB_VMMDEV_AGAIN;
  if (f->fail) return MULIB_VMMDEV_EIO;
  switch (r->operation) {
  case MULIB_VIRTIO_SERVICE_ENTROPY:
    memset(r->output, 0x5a, r->output_capacity); *used = r->output_capacity; break;
  case MULIB_VIRTIO_SERVICE_READ:
    if (r->device_id == MULIB_VIRTIO_RTC) {
      assert(r->output_capacity == 8);
      mulib_vmmdev_store64(r->output, f->clock_ns); *used = f->short_reply ? 7 : 8; break;
    }
    assert(r->address + r->output_capacity <= sizeof f->disk);
    memcpy(r->output, f->disk + r->address, r->output_capacity); *used = r->output_capacity; break;
  case MULIB_VIRTIO_SERVICE_WRITE:
    assert(r->address + r->input_size <= sizeof f->disk);
    memcpy(f->disk + r->address, r->input, r->input_size); break;
  case MULIB_VIRTIO_SERVICE_ID:
    memset(r->output, 'X', 20); *used = 20; break;
  case MULIB_VIRTIO_SERVICE_SEND:
    assert(r->input_size <= sizeof f->packet);
    memcpy(f->packet, r->input, r->input_size); f->packet_size = r->input_size; break;
  case MULIB_VIRTIO_SERVICE_RECEIVE:
    if (!f->packet_size) return MULIB_VMMDEV_AGAIN;
    if (r->output_capacity < f->packet_size) return MULIB_VMMDEV_ENOSPC;
    memcpy(r->output, f->packet, f->packet_size); *used = f->packet_size; f->packet_size = 0; break;
  case MULIB_VIRTIO_SERVICE_INPUT_CONFIG:
    if (r->address == 1) { memcpy(r->output, "keyboard", 8); *used = 8; } break;
  case MULIB_VIRTIO_SERVICE_GPIO:
    if (r->flags == 3) f->direction = mulib_endian_load_le32(r->input);
    if (r->flags == 5) f->value = mulib_endian_load_le32(r->input);
    if (r->flags == 2 || r->flags == 4) {
      r->output[0] = (unsigned char)(r->flags == 2 ? f->direction : f->value); *used = 1;
    }
    break;
  case MULIB_VIRTIO_SERVICE_I2C:
    if (r->flags & 2) { memset(r->output, 0x36, r->output_capacity); *used = r->output_capacity; }
    break;
  case MULIB_VIRTIO_SERVICE_CAN_CONTROL:
    assert(r->flags == 0x201 || r->flags == 0x202); break;
  case MULIB_VIRTIO_SERVICE_SPI:
    assert(r->input_size >= 32 && r->address == r->input[0]);
    for (size_t i = 0; i < r->output_capacity; ++i)
      r->output[i] = (unsigned char)(r->input_size > 32 ? r->input[32 + i] ^ 0xa5 : 0x36);
    *used = r->output_capacity;
    if (f->short_reply && *used) --*used;
    break;
  case MULIB_VIRTIO_SERVICE_SCSI_COMMAND:
    assert(r->input_size >= 51 && r->flags == 96);
    memset(r->output, 0, 108); *used = 108; break;
  case MULIB_VIRTIO_SERVICE_SCSI_TMF:
  case MULIB_VIRTIO_SERVICE_IOMMU:
    r->output[0] = 0; *used = 1; break;
  case MULIB_VIRTIO_SERVICE_PCM_CONTROL:
    assert(r->address < 2); break;
  case MULIB_VIRTIO_SERVICE_CIPHER:
    /* A known-vector provider fixture, not a cryptographic implementation. */
    assert(r->key_size == 16 && r->key[0] == 0x2b && r->input_size == 32 && r->output_capacity == 16);
    {
      static const unsigned char encrypted[16] = {0x76,0x49,0xab,0xac,0x81,0x19,0xb2,0x46,0xce,0xe9,0x8e,0x9b,0x12,0xe9,0x19,0x7d};
      static const unsigned char plain[16] = {0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a};
      assert(!memcmp(r->input + 16, r->flags ? encrypted : plain, 16));
      memcpy(r->output, r->flags ? plain : encrypted, 16); *used = 16;
    }
    break;
  case MULIB_VIRTIO_SERVICE_RPMB:
    memset(r->output, 0, r->output_capacity);
    for (size_t i = 0; i < r->output_capacity; i += 512) {
      r->output[i + 509] = 7; /* Physical provider reports no authentication key. */
      r->output[i + 510] = (unsigned char)r->flags;
      memcpy(r->output + i + 484, r->input + 484, 16);
    }
    *used = r->output_capacity; break;
  case MULIB_VIRTIO_SERVICE_GPU:
    if (r->flags == 0x100) {
      assert(r->output_capacity == 384); memset(r->output, 0, 384);
      mulib_endian_store_le32(r->output + 8, 640); mulib_endian_store_le32(r->output + 12, 480);
      mulib_endian_store_le32(r->output + 16, 1); *used = 384;
    }
    if (r->flags == 0x106) assert(r->memory == &f->bus.memory);
    break;
  case MULIB_VIRTIO_SERVICE_REQUEST:
    if (r->device_id == MULIB_VIRTIO_NSM) {
      /* CBOR {"GetRandom":{"random":h'36'}}; deterministic fixture, not entropy. */
      static const unsigned char response[] = {
        0xa1,0x69,'G','e','t','R','a','n','d','o','m',0xa1,0x66,'r','a','n','d','o','m',0x41,0x36
      };
      assert(r->output_capacity >= sizeof response);
      memcpy(r->output, response, sizeof response); *used = f->short_reply ? sizeof response - 1 : sizeof response;
    } else if (r->device_id == MULIB_VIRTIO_9P) {
      memcpy(r->output, r->input, 7); r->output[4] = (unsigned char)(r->input[4] + 1);
      mulib_endian_store_le32(r->output, 7); *used = 7;
    } else if (r->device_id == MULIB_VIRTIO_FS && r->output_capacity) {
      memset(r->output, 0, 16); mulib_endian_store_le32(r->output, 16);
      memcpy(r->output + 8, r->input + 8, 8); *used = 16;
    } else if (r->device_id == MULIB_VIRTIO_SCMI) {
      memcpy(r->output, r->input, 4); mulib_endian_store_le32(r->output + 4, 0); *used = 8;
    }
    break;
  default: break;
  }
  return 0;
}
static void init(fixture_t *f, unsigned id, const unsigned char *config, size_t size, uint64_t features)
{
  mulib_virtio_os_options_t o;
  mulib_vmmdev_memory_t memory;
  memset(f, 0, sizeof *f); memset(&o, 0, sizeof o);
  o.instance = 42; o.device_id = id; o.features = features;
  o.config = config; o.config_size = size; o.transfer_limit = sizeof f->input;
  o.input_scratch = f->input; o.output_scratch = f->output; o.service = service; o.context = f;
  assert(!mulib_virtio_os_init(&f->device, &o));
  f->devices[0] = &f->device;
  f->region.address = 0x1000; f->region.size = sizeof f->ram; f->region.data = f->ram; f->region.permissions = 3;
  memory.regions = &f->region; memory.count = 1;
  assert(!mulib_virtio_os_bus_init(&f->bus, f->devices, 1, memory));
  assert(!mulib_virtio_guest_attach(&f->guest, mulib_virtio_os_exchange, &f->bus, 42, features,
                                   0x1000, f->ram, sizeof f->ram));
  assert(f->guest.status == 11 && f->device.status == 11 && f->device.epoch == 2);
  assert(!mulib_virtio_guest_start(&f->guest));
}
static int raw(fixture_t *f, unsigned op, uint32_t epoch, uint64_t arg, uint32_t count,
               const void *payload, size_t n, mulib_vmmdev_message_t *r)
{
  unsigned char req[2048];
  static unsigned char rep[2048];
  mulib_vmmdev_message_t m = {0};
  size_t ns = sizeof req, nr = sizeof rep;
  m.opcode = (uint16_t)op; m.device = 42; m.epoch = epoch; m.token = 77;
  m.argument = arg; m.count = count; m.length = (uint32_t)n; m.payload = (const unsigned char *)payload;
  assert(!mulib_vmmdev_encode(&m, req, &ns));
  assert(!mulib_virtio_os_exchange(&f->bus, req, ns, rep, &nr));
  assert(!mulib_vmmdev_decode(rep, nr, r)); return (int)r->argument;
}
static int corrupt_exchange(void *context, const void *request, size_t n, void *response, size_t *capacity)
{
  fixture_t *f = (fixture_t *)context;
  unsigned char *p = (unsigned char *)response;
  int rc = mulib_virtio_os_exchange(&f->bus, request, n, response, capacity);
  if (rc) return rc;
  if (f->corrupt_reply == 1) p[16] ^= 1;
  if (f->corrupt_reply == 2) p[12] ^= 1;
  if (f->corrupt_reply == 3) mulib_endian_store_le32(p + 32, UINT32_MAX);
  if (f->corrupt_reply == 4) mulib_endian_store_le32(p + 36, 1);
  if (f->corrupt_reply == 5) mulib_endian_store_le32(p + 60, 1); /* RNG config must be empty. */
  if (f->corrupt_reply == 6) mulib_endian_store_le32(p + 44, 256); /* Status is a byte. */
  if (f->corrupt_reply == 7) mulib_endian_store_le32(p + 44, 16); /* Reserved status bit. */
  if (f->corrupt_reply == 8) mulib_endian_store_le32(p + 56, 2); /* RNG has one queue. */
  if (f->corrupt_reply == 9) mulib_endian_store_le32(p + 68, 511);
  return 0;
}
static void isolation(void)
{
  fixture_t f;
  mulib_virtio_os_bus_t bus, previous;
  mulib_vmmdev_region_t regions[2];
  mulib_vmmdev_memory_t memory = {regions, 2};
  unsigned char request[40], response[72];
  mulib_vmmdev_message_t m = {0};
  size_t n, size = sizeof request;
  unsigned calls;
  init(&f, MULIB_VIRTIO_RNG, NULL, 0, 0);
  m.opcode = MULIB_VMMDEV_RESET; m.device = 42; m.epoch = f.device.epoch;
  assert(!mulib_vmmdev_encode(&m, request, &size));
  calls = f.calls;
  {
    void *forbidden[] = {f.ram, f.input, f.output, &f.device, &f.bus, f.devices, &f.region};
    for (size_t i = 0; i < sizeof forbidden / sizeof forbidden[0]; ++i) {
      n = sizeof response;
      assert(mulib_virtio_os_exchange(&f.bus, request, size, forbidden[i], &n) == MULIB_VMMDEV_EINVAL);
      assert(n == sizeof response && f.calls == calls && f.device.epoch == m.epoch);
    }
  }
  memcpy(f.ram, request, size); n = sizeof response;
  assert(mulib_virtio_os_exchange(&f.bus, f.ram, size, response, &n) == MULIB_VMMDEV_EINVAL);
  memcpy(f.input, request, size); n = sizeof response;
  assert(mulib_virtio_os_exchange(&f.bus, f.input, size, response, &n) == MULIB_VMMDEV_EINVAL);
  n = size;
  assert(mulib_virtio_os_exchange(&f.bus, request, size, request, &n) == MULIB_VMMDEV_EINVAL);
  assert(mulib_virtio_os_exchange(&f.bus, request, size, response, &f.device.limit) == MULIB_VMMDEV_EINVAL);
  assert(f.device.limit == sizeof f.input);
  {
    union { size_t capacity; unsigned char bytes[80]; } storage;
    storage.capacity = sizeof storage.bytes;
    assert(mulib_virtio_os_exchange(&f.bus, request, size, storage.bytes, &storage.capacity) == MULIB_VMMDEV_EINVAL);
    assert(storage.capacity == sizeof storage.bytes);
  }
  assert(f.calls == calls && f.device.epoch == m.epoch);

  /* Every form of endpoint metadata must remain outside guest RAM. */
  memset(&bus, 0xa5, sizeof bus); memcpy(&previous, &bus, sizeof bus);
  regions[0] = f.region;
  regions[1].address = 0x10000; regions[1].permissions = 3;
  {
    void *forbidden[] = {&f.device, &bus, f.devices, regions, f.input, f.output};
    const size_t sizes[] = {sizeof f.device, sizeof bus, sizeof f.devices, sizeof regions,
                            sizeof f.input, sizeof f.output};
    for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; ++i) {
      regions[1].data = (unsigned char *)forbidden[i]; regions[1].size = sizes[i];
      assert(mulib_virtio_os_bus_init(&bus, f.devices, 1, memory) == MULIB_VMMDEV_EINVAL);
      assert(!memcmp(&bus, &previous, sizeof bus));
    }
  }
  /* Valid adjacent host buffers are not mistaken for aliases. */
  n = sizeof response;
  assert(!mulib_virtio_os_exchange(&f.bus, request, size, response, &n));
  assert(f.calls == calls + 1 && f.device.epoch == m.epoch + 1);
}
static void reset_cleanup(void)
{
  fixture_t f;
  mulib_vmmdev_message_t r;
  uint32_t epoch, generation;
  unsigned char config[56] = {0};
  unsigned char request[24] = {0}, response[16];
  size_t n;
  init(&f, MULIB_VIRTIO_RNG, NULL, 0, 0);
  memset(f.input, 0xa5, sizeof f.input); memset(f.output, 0x5a, sizeof f.output);
  epoch = f.device.epoch; generation = f.device.generation;
  f.reset_result = MULIB_VMMDEV_EIO;
  assert(mulib_virtio_guest_reset(&f.guest) == MULIB_VMMDEV_EIO);
  assert(f.device.epoch == epoch && f.device.generation == generation);
  assert(f.device.status & MULIB_VIRTIO_NEEDS_RESET);
  assert(f.input[0] == 0xa5 && f.output[0] == 0x5a);
  f.reset_result = 0;
  assert(!mulib_virtio_guest_reset(&f.guest));
  for (size_t i = 0; i < sizeof f.input; ++i) assert(!f.input[i] && !f.output[i]);
  assert(f.device.epoch == epoch + 1 && !f.device.status && !f.device.negotiated);
  assert(f.device.generation == generation + 1);

  /* Exhaustion must not invoke RESET; host retirement must still release it. */
  f.device.epoch = UINT32_MAX;
  {
    unsigned calls = f.calls;
    assert(raw(&f, MULIB_VMMDEV_RESET, UINT32_MAX, 0, 0, NULL, 0, &r) == MULIB_VMMDEV_ESTATE);
    assert(f.calls == calls);
    f.reset_result = MULIB_VMMDEV_AGAIN;
    assert(mulib_virtio_os_shutdown(&f.device) == MULIB_VMMDEV_AGAIN && f.device.epoch == UINT32_MAX);
    f.reset_result = 0;
    assert(!mulib_virtio_os_shutdown(&f.device) && !f.device.epoch);
    calls = f.calls;
    assert(!mulib_virtio_os_shutdown(&f.device) && f.calls == calls);
    assert(raw(&f, MULIB_VMMDEV_PROBE, 0, 0, 0, NULL, 0, &r) == MULIB_VMMDEV_ENODEV);
  }

  /* Memory ownership is persistent, unlike scratch and negotiated state. */
  mulib_vmmdev_store64(config, 4096); mulib_vmmdev_store64(config + 16, 0x100000);
  mulib_vmmdev_store64(config + 24, 8192); mulib_vmmdev_store64(config + 32, 8192);
  mulib_vmmdev_store64(config + 48, 8192);
  init(&f, MULIB_VIRTIO_MEM, config, sizeof config, 0);
  mulib_vmmdev_store64(request + 8, 0x100000); mulib_vmmdev_store16(request + 16, 1);
  n = sizeof response;
  assert(!mulib_virtio_guest_mem_request(&f.guest, 0, request, sizeof request, response, &n));
  assert(!mulib_vmmdev_load16(response) && f.device.plugged[0] == 1);
  assert(!mulib_virtio_guest_reset(&f.guest));
  assert(f.device.plugged[0] == 1 && mulib_vmmdev_load64(f.device.config + 40) == 4096);
}
static void frontend_validation(void)
{
  fixture_t f;
  mulib_virtio_guest_t guest, zero = {0};
  mulib_vmmdev_span_t spans[2] = {{0x1000,16,1},{0x1008,16,1}};
  unsigned calls;
  size_t used;
  init(&f, MULIB_VIRTIO_RNG, NULL, 0, 0);
  calls = f.calls;
  for (unsigned i = 5; i <= 9; ++i) {
    f.corrupt_reply = i;
    memset(&guest, 0xa5, sizeof guest);
    assert(mulib_virtio_guest_attach(&guest, corrupt_exchange, &f, 42, 0,
                                   0x1000, f.ram, sizeof f.ram) == MULIB_VMMDEV_EIO);
    assert(!memcmp(&guest, &zero, sizeof guest));
    assert(f.calls == calls && f.device.epoch == 2); /* Bad discovery cannot reset the provider. */
  }
  {
    uint64_t token = f.guest.token;
    assert(mulib_virtio_guest_submit(&f.guest, 0, spans, 2, &used) == MULIB_VMMDEV_EINVAL);
    assert(!used && f.guest.token == token && f.calls == calls);
    spans[1].address = 0x1010;
    assert(!mulib_virtio_guest_submit(&f.guest, 0, spans, 2, &used) && used == 32);
  }
}
static void transport(void)
{
  fixture_t f;
  mulib_vmmdev_message_t r;
  unsigned char out[16], descriptors[32];
  size_t n;
  unsigned calls;
  init(&f, 4, NULL, 0, 0);
  n = sizeof out; assert(!mulib_virtio_guest_entropy(&f.guest, out, &n));
  assert(n == sizeof out && out[0] == 0x5a);
  f.again = 1; memset(out, 0xaa, sizeof out); n = sizeof out;
  assert(mulib_virtio_guest_entropy(&f.guest, out, &n) == MULIB_VMMDEV_AGAIN && out[0] == 0xaa);
  f.again = 0;
  f.guest.exchange = corrupt_exchange; f.guest.context = &f;
  for (unsigned i = 1; i <= 4; ++i) {
    f.corrupt_reply = i; n = sizeof out; memset(out, 0xaa, sizeof out);
    assert(mulib_virtio_guest_entropy(&f.guest, out, &n));
    assert(out[0] == 0xaa && n == sizeof out);
  }
  f.guest.exchange = mulib_virtio_os_exchange; f.guest.context = &f.bus;
  calls = f.calls;
  assert(raw(&f, MULIB_VMMDEV_SUBMIT, 1, 0, 0, NULL, 0, &r) == MULIB_VMMDEV_ESTALE);
  assert(raw(&f, MULIB_VMMDEV_STATUS, 2, 3, 0, NULL, 0, &r) == MULIB_VMMDEV_ESTATE);
  assert(raw(&f, MULIB_VMMDEV_FEATURES, 2, MULIB_VIRTIO_VERSION_1, 0, NULL, 0, &r) == MULIB_VMMDEV_ESTATE);
  assert(f.calls == calls);
  mulib_vmmdev_store64(descriptors, 0x1000); mulib_endian_store_le32(descriptors + 8, 16);
  mulib_endian_store_le32(descriptors + 12, 1);
  mulib_vmmdev_store64(descriptors + 16, 0x1008); mulib_endian_store_le32(descriptors + 24, 16);
  mulib_endian_store_le32(descriptors + 28, 1);
  assert(raw(&f, MULIB_VMMDEV_SUBMIT, 2, 0, 2, descriptors, 32, &r) == MULIB_VMMDEV_EINVAL);
  assert(f.calls == calls);
  assert(!mulib_virtio_guest_reset(&f.guest));
  assert(f.device.epoch == 3 && f.device.status == 0);
  n = sizeof out; assert(mulib_virtio_guest_entropy(&f.guest, out, &n) == MULIB_VMMDEV_ESTATE);
  assert(raw(&f, MULIB_VMMDEV_STATUS, 3, 15, 0, NULL, 0, &r) == MULIB_VMMDEV_ESTATE);
  assert(!raw(&f, MULIB_VMMDEV_STATUS, 3, 1, 0, NULL, 0, &r));
  assert(!raw(&f, MULIB_VMMDEV_STATUS, 3, 3, 0, NULL, 0, &r));
  assert(raw(&f, MULIB_VMMDEV_FEATURES, 3, 0, 0, NULL, 0, &r) == MULIB_VMMDEV_ENOTSUP);
  assert(raw(&f, MULIB_VMMDEV_FEATURES, 3, MULIB_VIRTIO_VERSION_1 | 1, 0, NULL, 0, &r) == MULIB_VMMDEV_ENOTSUP);
  assert(raw(&f, MULIB_VMMDEV_STATUS, 3, 11, 0, NULL, 0, &r) == MULIB_VMMDEV_ESTATE);
  /* A short response buffer must not execute a reset. */
  {
    unsigned char req[40], rep[40]; size_t ns = 40, nr = 39;
    mulib_vmmdev_message_t m = {0};
    m.opcode = MULIB_VMMDEV_RESET; m.device = 42; m.epoch = 3;
    assert(!mulib_vmmdev_encode(&m, req, &ns)); calls = f.calls;
    assert(mulib_virtio_os_exchange(&f.bus, req, ns, rep, &nr) == MULIB_VMMDEV_ENOSPC);
    assert(f.calls == calls && f.device.epoch == 3);
  }
}
static void block(void)
{
  fixture_t f;
  unsigned char config[8], data[512], copy[512];
  mulib_vmmdev_store64(config, 8);
  init(&f, 2, config, 8, UINT64_C(1) << 9);
  memset(data, 0x93, sizeof data);
  assert(!mulib_virtio_guest_block(&f.guest, 1, 7, data, sizeof data));
  assert(!mulib_virtio_guest_block(&f.guest, 0, 7, copy, sizeof copy));
  assert(!memcmp(data, copy, sizeof data));
  assert(mulib_virtio_guest_block(&f.guest, 0, 8, copy, sizeof copy) == MULIB_VMMDEV_EIO);
  assert(!mulib_virtio_guest_block(&f.guest, 4, 0, NULL, 0));
  assert(!mulib_virtio_guest_block(&f.guest, 8, 0, copy, 20) && copy[0] == 'X');
  /* Headers and responses may cross arbitrary descriptor boundaries. */
  {
    mulib_vmmdev_span_t spans[4] = {{0x1000,8,0},{0x1020,8,0},{0x1100,256,1},{0x1400,257,1}};
    unsigned char header[16]; size_t used;
    mulib_virtio_blk_header(header, 0, 7);
    memcpy(f.ram, header, 8); memcpy(f.ram + 32, header + 8, 8);
    assert(!mulib_virtio_guest_submit(&f.guest, 0, spans, 4, &used) && used == 513);
    assert(!memcmp(f.ram + 256, data, 256) && !memcmp(f.ram + 1024, data + 256, 256));
    assert(!f.ram[1280]);
  }
  f.fail = 1; memset(copy, 0x77, sizeof copy);
  assert(mulib_virtio_guest_block(&f.guest, 0, 0, copy, sizeof copy) == MULIB_VMMDEV_EIO && copy[0] == 0x77);
  init(&f, 2, config, 8, UINT64_C(1) << 5);
  assert(mulib_virtio_guest_block(&f.guest, 1, 0, data, sizeof data) == MULIB_VMMDEV_EIO);
  assert(mulib_virtio_guest_block(&f.guest, 4, 0, NULL, 0) == MULIB_VMMDEV_ENOTSUP);
}
static void streams(void)
{
  fixture_t f;
  unsigned char config[136] = {0}, packet[32] = {0}, out[64];
  const unsigned ids[] = {1, 3, 7, 18};
  for (unsigned i = 0; i < 4; ++i) {
    unsigned id = ids[i]; size_t n = sizeof out;
    size_t cfgsize = id == 1 ? 6 : (id == 18 ? 136 : 0);
    memset(packet, 0, sizeof packet);
    if (id == 7) { mulib_vmmdev_store16(packet + 12, 16); packet[16] = 0x4a; }
    else packet[20] = 0x4a;
    init(&f, id, config, cfgsize, 0);
    assert(!mulib_virtio_guest_send(&f.guest, packet, sizeof packet));
    assert(!mulib_virtio_guest_receive(&f.guest, out, &n) && n == sizeof packet);
    assert(!memcmp(out, packet, n));
    n = sizeof out; assert(mulib_virtio_guest_receive(&f.guest, out, &n) == MULIB_VMMDEV_AGAIN);
    if (id == 18) {
      unsigned char select[2] = {1, 0};
      assert(!mulib_virtio_guest_config_write(&f.guest, 0, select, 2));
      assert(!mulib_virtio_guest_config_read(&f.guest, 0, config, 136));
      assert(config[2] == 8 && !memcmp(config + 8, "keyboard", 8));
    }
  }
}
static void devices(void)
{
  fixture_t f;
  unsigned char config[56] = {0}, request[64] = {0}, response[64];
  size_t n;
  uint32_t value;
  init(&f, 5, config, 8, UINT64_C(1) << 1);
  mulib_endian_store_le32(request, 123); n = 0;
  assert(!mulib_virtio_guest_transact(&f.guest, 0, request, 4, NULL, &n));
  assert(!mulib_virtio_guest_transact(&f.guest, 1, request, 4, NULL, &n));
  assert(!mulib_virtio_os_request_memory(&f.device, 100));
  assert(!mulib_virtio_guest_config_read(&f.guest, 0, response, 8) && response[0] == 100);
  mulib_endian_store_le32(request, 1);
  assert(!mulib_virtio_guest_config_write(&f.guest, 4, request, 4));

  mulib_vmmdev_store64(config, 0x100000); mulib_vmmdev_store64(config + 8, 0x1000);
  init(&f, 27, config, 16, 0); assert(!mulib_virtio_guest_pmem_flush(&f.guest));
  f.fail = 1; assert(mulib_virtio_guest_pmem_flush(&f.guest) == MULIB_VMMDEV_EIO);

  memset(config, 0, sizeof config); mulib_vmmdev_store16(config, 4);
  init(&f, 41, config, 8, 0); value = 1;
  assert(!mulib_virtio_guest_gpio(&f.guest, 5, 2, &value));
  assert(!mulib_virtio_guest_gpio(&f.guest, 3, 2, &value));
  value = 0; assert(!mulib_virtio_guest_gpio(&f.guest, 4, 2, &value) && value == 1);
  assert(!mulib_virtio_guest_gpio(&f.guest, 2, 2, &value) && value == 1);
  assert(mulib_virtio_guest_gpio(&f.guest, 2, 4, &value) == MULIB_VMMDEV_EIO);

  init(&f, 34, NULL, 0, 1);
  assert(!mulib_virtio_guest_i2c(&f.guest, 0xa0, 0, request, 3));
  assert(!mulib_virtio_guest_i2c(&f.guest, 0xa0, 2, response, 3) && response[0] == 0x36);
  assert(!mulib_virtio_guest_i2c(&f.guest, 0xa0, 2, NULL, 0));
  f.fail = 1; assert(mulib_virtio_guest_i2c(&f.guest, 0xa0, 1, request, 1) == MULIB_VMMDEV_EIO);
  f.fail = 0; { unsigned calls = f.calls;
    assert(mulib_virtio_guest_i2c(&f.guest, 0xa0, 2, response, 1) == MULIB_VMMDEV_EIO);
    assert(f.calls == calls);
  }
  assert(!mulib_virtio_guest_i2c(&f.guest, 0xa0, 2, response, 1));

  config[0] = 1; config[1] = 0; config[2] = 'x'; init(&f, 9, config, 3, 1);
  memset(request, 0, sizeof request); mulib_endian_store_le32(request, 7); request[4] = 100; request[5] = 5;
  n = sizeof response; assert(!mulib_virtio_guest_transact(&f.guest, 0, request, 7, response, &n));
  assert(n == 7 && response[4] == 101 && response[5] == 5);
  memset(config, 0, sizeof config); config[0] = 'x'; mulib_endian_store_le32(config + 36, 1);
  init(&f, 26, config, 40, 0);
  memset(request, 0, sizeof request); mulib_endian_store_le32(request, 40);
  mulib_endian_store_le32(request + 4, 26); mulib_vmmdev_store64(request + 8, 321);
  n = sizeof response; assert(!mulib_virtio_guest_transact(&f.guest, 1, request, 40, response, &n));
  assert(n == 16 && mulib_vmmdev_load64(response + 8) == 321);
  mulib_endian_store_le32(request + 4, 2); n = 0;
  assert(!mulib_virtio_guest_transact(&f.guest, 0, request, 40, NULL, &n));
  init(&f, 32, NULL, 0, 0); memset(request, 0, 4); n = sizeof response;
  assert(!mulib_virtio_guest_transact(&f.guest, 0, request, 4, response, &n) && n == 8);
}
static void memory_device(void)
{
  fixture_t f;
  unsigned char cfg[56] = {0}, req[24] = {0}, rep[10];
  size_t n;
  mulib_vmmdev_store64(cfg, 4096); mulib_vmmdev_store64(cfg + 16, 0x100000);
  mulib_vmmdev_store64(cfg + 24, 4 * 4096); mulib_vmmdev_store64(cfg + 32, 4 * 4096);
  mulib_vmmdev_store64(cfg + 48, 2 * 4096); init(&f, 24, cfg, 56, 0);
  mulib_vmmdev_store64(req + 8, 0x100000); mulib_vmmdev_store16(req + 16, 2);
  n = 10; assert(!mulib_virtio_guest_transact(&f.guest, 0, req, 24, rep, &n) && rep[0] == 0);
  req[0] = 3; n = 10;
  assert(!mulib_virtio_guest_transact(&f.guest, 0, req, 24, rep, &n) && rep[8] == 0);
  mulib_vmmdev_store16(req + 16, 3); n = 10;
  assert(!mulib_virtio_guest_transact(&f.guest, 0, req, 24, rep, &n) && rep[8] == 2);
  req[0] = 0; mulib_vmmdev_store64(req + 8, 0x102000); mulib_vmmdev_store16(req + 16, 1); n = 10;
  assert(!mulib_virtio_guest_transact(&f.guest, 0, req, 24, rep, &n) && rep[0] == 1);
  assert(!mulib_virtio_os_request_memory(&f.device, 4 * 4096));
  f.again = 1; n = 10;
  assert(!mulib_virtio_guest_transact(&f.guest, 0, req, 24, rep, &n) && rep[0] == 2);
  f.again = 0; n = 10;
  assert(!mulib_virtio_guest_transact(&f.guest, 0, req, 24, rep, &n) && rep[0] == 0);
  req[0] = 2; n = 10;
  assert(!mulib_virtio_guest_transact(&f.guest, 0, req, 24, rep, &n) && rep[0] == 0);
  assert(!mulib_virtio_guest_config_read(&f.guest, 0, cfg, 56) && !mulib_vmmdev_load64(cfg + 40));
  /* Count*block must not wrap and let an oversized range index the bitmap. */
  memset(cfg, 0, sizeof cfg); mulib_vmmdev_store64(cfg, UINT64_C(1) << 63);
  mulib_vmmdev_store64(cfg + 24, UINT64_C(1) << 63); mulib_vmmdev_store64(cfg + 32, UINT64_C(1) << 63);
  mulib_vmmdev_store64(cfg + 48, UINT64_C(1) << 63); init(&f, 24, cfg, 56, 0);
  memset(req, 0, sizeof req); mulib_vmmdev_store16(req + 16, UINT16_MAX); n = 10;
  assert(!mulib_virtio_guest_transact(&f.guest, 0, req, 24, rep, &n) && rep[0] == 3);
}
static void scsi_iommu_sound(void)
{
  fixture_t f;
  unsigned char cfg[56] = {0}, req[128] = {0}, rep[256];
  size_t n;
  mulib_endian_store_le32(cfg, 1); mulib_endian_store_le32(cfg + 4, 62);
  mulib_endian_store_le32(cfg + 8, 8); mulib_endian_store_le32(cfg + 12, 1);
  mulib_endian_store_le32(cfg + 16, 16); mulib_endian_store_le32(cfg + 20, 96);
  mulib_endian_store_le32(cfg + 24, 32);
  init(&f, 8, cfg, 36, 0); req[0] = 1; req[19] = 0x12; n = 108;
  assert(!mulib_virtio_guest_transact(&f.guest, 2, req, 51, rep, &n) && n == 108 && !rep[11]);
  n = 109; assert(!mulib_virtio_guest_transact(&f.guest, 2, req, 52, rep, &n) && rep[11] == 9);
  memset(req, 0, sizeof req); req[4] = 5; req[8] = 1; n = 1;
  assert(!mulib_virtio_guest_transact(&f.guest, 0, req, 24, rep, &n) && !rep[0]);
  mulib_endian_store_le32(req, 256);
  assert(!mulib_virtio_guest_config_write(&f.guest, 24, req, 4));
  mulib_endian_store_le32(req, 257);
  assert(mulib_virtio_guest_config_write(&f.guest, 24, req, 4) == MULIB_VMMDEV_EBOUNDS);
  assert(!mulib_virtio_guest_reset(&f.guest));
  assert(mulib_endian_load_le32(f.device.config + 24) == 32);

  memset(cfg, 0, sizeof cfg); mulib_vmmdev_store64(cfg, 4096);
  init(&f, 23, cfg, 8, 4); memset(req, 0, sizeof req); req[0] = 1; req[8] = 3; n = 4;
  assert(!mulib_virtio_guest_transact(&f.guest, 0, req, 20, rep, &n) && !rep[0]);
  req[0] = 3; mulib_vmmdev_store64(req + 8, 0x1000); mulib_vmmdev_store64(req + 16, 0x1fff);
  mulib_vmmdev_store64(req + 24, 0x2000); mulib_endian_store_le32(req + 32, 3); n = 4;
  assert(!mulib_virtio_guest_transact(&f.guest, 0, req, 36, rep, &n) && !rep[0]);
  req[8] = 1; n = 4;
  assert(!mulib_virtio_guest_transact(&f.guest, 0, req, 36, rep, &n) && rep[0] == 5);
  req[0] = 5; n = 4;
  assert(!mulib_virtio_guest_transact(&f.guest, 0, req, 4, rep, &n) && rep[0] == 2);
  memset(f.packet, 0, 24); f.packet[0] = 2; f.packet_size = 24; n = 24;
  assert(!mulib_virtio_guest_transact(&f.guest, 1, NULL, 0, rep, &n) && rep[0] == 2);
  f.packet[4] = 0x80; f.packet_size = 24; n = 24;
  assert(mulib_virtio_guest_transact(&f.guest, 1, NULL, 0, rep, &n) == MULIB_VMMDEV_EIO);

  memset(cfg, 0, sizeof cfg); cfg[4] = 2; init(&f, 25, cfg, 12, 0);
  memset(req, 0, sizeof req); mulib_endian_store_le32(req, 0x100);
  mulib_endian_store_le32(req + 8, 2); mulib_endian_store_le32(req + 12, 32); n = sizeof rep;
  assert(!mulib_virtio_guest_transact(&f.guest, 0, req, 16, rep, &n) && n == 68);
  assert(mulib_endian_load_le32(rep) == 0x8000 && rep[28] == 0 && rep[60] == 1);
  for (unsigned stream = 0; stream < 2; ++stream) {
    memset(req, 0, sizeof req); mulib_endian_store_le32(req, 0x101); req[4] = (unsigned char)stream;
    mulib_endian_store_le32(req + 8, 32); mulib_endian_store_le32(req + 12, 16);
    req[20] = 2; req[21] = 5; req[22] = 7; n = 4;
    assert(!mulib_virtio_guest_transact(&f.guest, 0, req, 24, rep, &n) && mulib_endian_load_le32(rep) == 0x8000);
    mulib_endian_store_le32(req, 0x104); n = 4;
    assert(!mulib_virtio_guest_transact(&f.guest, 0, req, 8, rep, &n) && mulib_endian_load_le32(rep) == 0x8001);
    mulib_endian_store_le32(req, 0x102); n = 4;
    assert(!mulib_virtio_guest_transact(&f.guest, 0, req, 8, rep, &n) && mulib_endian_load_le32(rep) == 0x8000);
    mulib_endian_store_le32(req, 0x104); n = 4;
    assert(!mulib_virtio_guest_transact(&f.guest, 0, req, 8, rep, &n) && mulib_endian_load_le32(rep) == 0x8000);
    memset(req, 0, sizeof req); req[0] = (unsigned char)stream; n = stream ? 24 : 8;
    assert(!mulib_virtio_guest_transact(&f.guest, stream ? 3 : 2, req, stream ? 4 : 20, rep, &n));
    assert(mulib_endian_load_le32(rep + n - 8) == 0x8000);
  }
}
static void crypto_rpmb(void)
{
  fixture_t f;
  unsigned char cfg[56] = {0}, req[1024] = {0}, rep[1024];
  static const unsigned char key[16] = {0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c};
  static const unsigned char plain[16] = {0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a};
  uint64_t session;
  size_t n;
  cfg[0] = 1; cfg[4] = 1; cfg[8] = 1; cfg[12] = 8; cfg[36] = 32;
  mulib_vmmdev_store64(cfg + 48, 512); init(&f, 20, cfg, 56, 0);
  req[0] = 2; req[4] = 3; req[16] = 3; req[20] = 16; req[24] = 1; req[64] = 1;
  memcpy(req + 72, key, 16); n = 16;
  assert(!mulib_virtio_guest_transact(&f.guest, 1, req, 88, rep, &n) && !mulib_endian_load_le32(rep + 8));
  session = mulib_vmmdev_load64(rep); assert(session);
  for (unsigned i = 0; i < 88; ++i) assert(!f.input[i]);
  memset(req, 0, sizeof req); req[4] = 3; mulib_vmmdev_store64(req + 8, session);
  req[24] = 16; req[28] = 16; req[32] = 16; req[64] = 1;
  for (unsigned i = 0; i < 16; ++i) req[72 + i] = (unsigned char)i;
  memcpy(req + 88, plain, 16); n = 17;
  assert(!mulib_virtio_guest_transact(&f.guest, 0, req, 104, rep, &n) && !rep[16] && rep[0] == 0x76);
  req[0] = 1; n = 17;
  assert(!mulib_virtio_guest_transact(&f.guest, 0, req, 104, rep, &n) && rep[16] == 4);
  memset(req, 0, sizeof req); req[0] = 3; mulib_vmmdev_store64(req + 16, session); n = 1;
  assert(!mulib_virtio_guest_transact(&f.guest, 1, req, 72, rep, &n) && !rep[0]);
  for (unsigned i = 0; i < 32; ++i) assert(!f.device.sessions[0].key[i]);
  n = 1; assert(!mulib_virtio_guest_transact(&f.guest, 1, req, 72, rep, &n) && rep[0] == 4);

  memset(cfg, 0, sizeof cfg); cfg[0] = 1; cfg[1] = 2; cfg[2] = 2;
  init(&f, 28, cfg, 3, 0); memset(req, 0, sizeof req); req[507] = 1; req[511] = 2; req[484] = 0xa7; n = 512;
  assert(!mulib_virtio_guest_transact(&f.guest, 0, req, 512, rep, &n) && rep[510] == 2 && rep[509] == 7 && rep[484] == 0xa7);
  req[511] = 4; req[507] = 3; n = sizeof rep;
  assert(mulib_virtio_guest_transact(&f.guest, 0, req, 512, rep, &n) == MULIB_VMMDEV_EINVAL);
}
static void gpu_vsock(void)
{
  fixture_t f;
  unsigned char cfg[16] = {0}, req[128] = {0}, rep[512];
  size_t n;
  cfg[8] = 1; init(&f, 16, cfg, 16, 0);
  mulib_endian_store_le32(req, 0x100); req[4] = 1; mulib_vmmdev_store64(req + 8, 1234); n = sizeof rep;
  assert(!mulib_virtio_guest_transact(&f.guest, 0, req, 24, rep, &n) && n == 408);
  assert(mulib_endian_load_le32(rep) == 0x1101 && mulib_vmmdev_load64(rep + 8) == 1234);
  mulib_endian_store_le32(req, 0x101); req[24] = 9; req[28] = 1;
  mulib_endian_store_le32(req + 32, 16); mulib_endian_store_le32(req + 36, 16); n = 24;
  assert(!mulib_virtio_guest_transact(&f.guest, 0, req, 40, rep, &n) && mulib_endian_load_le32(rep) == 0x1100);
  n = 24; assert(!mulib_virtio_guest_transact(&f.guest, 0, req, 40, rep, &n) && mulib_endian_load_le32(rep) == 0x1203);
  memset(req + 24, 0, sizeof req - 24); mulib_endian_store_le32(req, 0x106); req[24] = 9; req[28] = 1;
  mulib_vmmdev_store64(req + 32, 0x2000); mulib_endian_store_le32(req + 40, 1024); n = 24;
  assert(!mulib_virtio_guest_transact(&f.guest, 0, req, 48, rep, &n) && mulib_endian_load_le32(rep) == 0x1100);
  memset(req + 24, 0, sizeof req - 24); mulib_endian_store_le32(req, 0x105);
  req[32] = 16; req[36] = 16; req[48] = 9; n = 24;
  assert(!mulib_virtio_guest_transact(&f.guest, 0, req, 56, rep, &n) && mulib_endian_load_le32(rep) == 0x1100);
  req[40] = 1; n = 24;
  assert(!mulib_virtio_guest_transact(&f.guest, 0, req, 56, rep, &n) && mulib_endian_load_le32(rep) == 0x1205);
  memset(req + 24, 0, sizeof req - 24); mulib_endian_store_le32(req, 0x102); req[24] = 9; n = 24;
  assert(!mulib_virtio_guest_transact(&f.guest, 0, req, 32, rep, &n) && mulib_endian_load_le32(rep) == 0x1100);
  memset(cfg, 0, sizeof cfg); cfg[0] = 5; init(&f, 19, cfg, 8, 0);
  memset(req, 0, sizeof req); mulib_vmmdev_store64(req, 5); mulib_vmmdev_store64(req + 8, 2);
  req[28] = 1; req[30] = 1; n = 0;
  assert(!mulib_virtio_guest_transact(&f.guest, 1, req, 44, NULL, &n));
  /* Socket provider's queued response is addressed to the guest. */
  mulib_vmmdev_store64(f.packet, 2); mulib_vmmdev_store64(f.packet + 8, 5); f.packet[30] = 2;
  n = sizeof rep; assert(!mulib_virtio_guest_transact(&f.guest, 0, NULL, 0, rep, &n) && n == 44 && rep[30] == 2);
  req[0] = 6; n = 0;
  assert(mulib_virtio_guest_transact(&f.guest, 1, req, 44, NULL, &n) == MULIB_VMMDEV_EINVAL);
}
static void can_device(void)
{
  fixture_t f;
  unsigned char config[2] = {0}, frame[80] = {1}, out[80];
  size_t n;
  unsigned calls;
  init(&f, MULIB_VIRTIO_CAN, config, sizeof config, 7);
  mulib_vmmdev_store16(frame + 2, 8); mulib_endian_store_le32(frame + 12, 0x123);
  n = 1;
  assert(!mulib_virtio_guest_can_request(&f.guest, 0, frame, 24, out, &n) && out[0] == 1);
  assert(!f.packet_size);
  assert(!mulib_virtio_guest_can_control(&f.guest, 1));
  n = 1; assert(!mulib_virtio_guest_can_request(&f.guest, 0, frame, 24, out, &n) && !out[0]);
  f.packet[1] = 1; n = sizeof out;
  assert(!mulib_virtio_guest_can_request(&f.guest, 1, NULL, 0, out, &n) && n == 24);
  calls = f.calls; frame[4] = 1; n = 1;
  assert(mulib_virtio_guest_can_request(&f.guest, 0, frame, 24, out, &n) == MULIB_VMMDEV_EINVAL);
  assert(f.calls == calls); frame[4] = 0;
  mulib_endian_store_le32(frame + 12, 0x800); n = 1;
  assert(mulib_virtio_guest_can_request(&f.guest, 0, frame, 24, out, &n) == MULIB_VMMDEV_EINVAL);
  mulib_endian_store_le32(frame + 8, 0xc000); /* extended FD */
  mulib_vmmdev_store16(frame + 2, 64); n = 1;
  assert(!mulib_virtio_guest_can_request(&f.guest, 0, frame, 80, out, &n) && !out[0]);
  mulib_endian_store_le32(frame + 8, 0xe000); n = 1; /* FD + RTR invalid */
  assert(mulib_virtio_guest_can_request(&f.guest, 0, frame, 80, out, &n) == MULIB_VMMDEV_EINVAL);
  assert(!mulib_virtio_os_can_busoff(&f.device, 1) && !f.device.can_started);
  assert(!mulib_virtio_guest_config_read(&f.guest, 0, config, 2) && config[0] == 1);
  n = sizeof out; assert(mulib_virtio_guest_can_request(&f.guest, 1, NULL, 0, out, &n) == MULIB_VMMDEV_AGAIN);
  f.again = 1; assert(mulib_virtio_guest_can_control(&f.guest, 1) == MULIB_VMMDEV_AGAIN);
  assert(!f.device.can_started); f.again = 0;
  assert(!mulib_virtio_guest_can_control(&f.guest, 1) && f.device.can_started);
  assert(!mulib_virtio_guest_can_control(&f.guest, 0) && !f.device.can_started);
  assert(!mulib_virtio_guest_can_control(&f.guest, 1));
  assert(!mulib_virtio_guest_reset(&f.guest) && !f.device.can_started);
  config[0] = 0; init(&f, MULIB_VIRTIO_CAN, config, sizeof config, 1);
  assert(!mulib_virtio_guest_can_control(&f.guest, 1));
  mulib_endian_store_le32(frame + 8, 0x4000); n = 1;
  assert(mulib_virtio_guest_can_request(&f.guest, 0, frame, 80, out, &n) == MULIB_VMMDEV_EINVAL);
}

static void bt_device(void)
{
  fixture_t f;
  unsigned char config[5] = {0}, command[8] = {1,3,12,0}, out[32];
  size_t n;
  unsigned calls;
  init(&f, MULIB_VIRTIO_BT, config, sizeof config, 0);
  assert(!mulib_virtio_guest_send(&f.guest, command, 4) && f.packet_size == 4);
  calls = f.calls; command[3] = 1;
  assert(mulib_virtio_guest_send(&f.guest, command, 4) == MULIB_VMMDEV_EINVAL && f.calls == calls);
  command[3] = 0; command[0] = 4;
  assert(mulib_virtio_guest_send(&f.guest, command, 3) == MULIB_VMMDEV_EINVAL);
  f.packet[0] = 4; f.packet[1] = 0x0e; f.packet[2] = 0; f.packet_size = 3;
  n = sizeof out; assert(!mulib_virtio_guest_receive(&f.guest, out, &n) && n == 3 && out[0] == 4);
  f.packet[0] = 4; f.packet[2] = 1; f.packet_size = 3;
  n = sizeof out; memset(out, 0x77, sizeof out);
  assert(mulib_virtio_guest_receive(&f.guest, out, &n) == MULIB_VMMDEV_EIO && out[0] == 0x77);
  command[0] = 2; command[3] = 0; command[4] = 0;
  assert(!mulib_virtio_guest_send(&f.guest, command, 5));
  command[0] = 3; assert(!mulib_virtio_guest_send(&f.guest, command, 4));
  command[0] = 5; assert(!mulib_virtio_guest_send(&f.guest, command, 5));
  command[4] = 0x80;
  assert(mulib_virtio_guest_send(&f.guest, command, 5) == MULIB_VMMDEV_EINVAL);
  f.again = 1; n = sizeof out;
  assert(mulib_virtio_guest_receive(&f.guest, out, &n) == MULIB_VMMDEV_AGAIN);
}

static void spi_device(void)
{
  fixture_t f;
  unsigned char config[32] = {2,1,7,7}, header[32] = {1,8,1,1,1};
  unsigned char tx[4] = {1,2,3,4}, rx[4];
  unsigned calls;
  mulib_endian_store_le32(config + 4, 0x80000080u); /* 8 or 32 bits per word */
  mulib_endian_store_le32(config + 8, 127);
  mulib_endian_store_le32(config + 12, 1000000);
  mulib_endian_store_le32(header + 12, 500000);
  init(&f, MULIB_VIRTIO_SPI, config, sizeof config, 0);
  assert(!mulib_virtio_guest_spi_transfer(&f.guest, header, tx, 4, rx, 4) && rx[0] == (1 ^ 0xa5));
  assert(!mulib_virtio_guest_spi_transfer(&f.guest, header, NULL, 0, rx, 4) && rx[0] == 0x36);
  assert(!mulib_virtio_guest_spi_transfer(&f.guest, header, tx, 4, NULL, 0));
  calls = f.calls; header[0] = 2;
  assert(mulib_virtio_guest_spi_transfer(&f.guest, header, tx, 4, rx, 4) == MULIB_VMMDEV_EINVAL);
  assert(f.calls == calls); header[0] = 1;
  header[1] = 32;
  assert(!mulib_virtio_guest_spi_transfer(&f.guest, header, tx, 4, rx, 4));
  assert(mulib_virtio_guest_spi_transfer(&f.guest, header, tx, 3, rx, 3) == MULIB_VMMDEV_EINVAL);
  header[1] = 8; header[5] = 1;
  assert(mulib_virtio_guest_spi_transfer(&f.guest, header, tx, 4, rx, 4) == MULIB_VMMDEV_EINVAL);
  header[5] = 0; header[16] = 1;
  assert(mulib_virtio_guest_spi_transfer(&f.guest, header, tx, 4, rx, 4) == MULIB_VMMDEV_EINVAL);
  header[16] = 0; f.fail = 1; memset(rx, 0x77, sizeof rx);
  assert(mulib_virtio_guest_spi_transfer(&f.guest, header, tx, 4, rx, 4) == MULIB_VMMDEV_EIO && rx[0] == 0x77);
  f.fail = 0; f.short_reply = 1;
  assert(mulib_virtio_guest_spi_transfer(&f.guest, header, tx, 4, rx, 4) == MULIB_VMMDEV_EIO);
  f.short_reply = 0; f.again = 1;
  assert(mulib_virtio_guest_spi_transfer(&f.guest, header, tx, 4, rx, 4) == MULIB_VMMDEV_AGAIN);
}

static void rtc_device(void)
{
  fixture_t f;
  unsigned char request[24] = {0}, response[32];
  uint64_t ns = 123;
  size_t n;
  init(&f, MULIB_VIRTIO_RTC, NULL, 0, 0);
  f.clock_ns = 1000000000;
  assert(!mulib_virtio_guest_rtc_read(&f.guest, &ns) && ns == f.clock_ns);
  --f.clock_ns;
  assert(mulib_virtio_guest_rtc_read(&f.guest, &ns) == MULIB_VMMDEV_EIO && ns == 1000000000);
  f.clock_ns += 2; f.short_reply = 1;
  assert(mulib_virtio_guest_rtc_read(&f.guest, &ns) == MULIB_VMMDEV_EIO);
  f.short_reply = 0; f.again = 1;
  assert(mulib_virtio_guest_rtc_read(&f.guest, &ns) == MULIB_VMMDEV_AGAIN); f.again = 0;
  mulib_vmmdev_store16(request, 0x1000); n = sizeof response;
  assert(!mulib_virtio_guest_rtc_request(&f.guest, 0, request, 8, response, &n)
         && n == 16 && response[8] == 1);
  mulib_vmmdev_store16(request, 0x1001); n = sizeof response;
  assert(!mulib_virtio_guest_rtc_request(&f.guest, 0, request, 16, response, &n)
         && n == 16 && response[8] == 2);
  mulib_vmmdev_store16(request, 0x1002); n = sizeof response;
  assert(!mulib_virtio_guest_rtc_request(&f.guest, 0, request, 16, response, &n)
         && n == 16 && !response[8]);
  mulib_vmmdev_store16(request, 2); n = sizeof response;
  assert(!mulib_virtio_guest_rtc_request(&f.guest, 0, request, 16, response, &n) && response[0] == 2);
  mulib_vmmdev_store16(request, 0x1003); n = sizeof response;
  assert(!mulib_virtio_guest_rtc_request(&f.guest, 0, request, 16, response, &n) && response[0] == 3);
  mulib_vmmdev_store16(request, 1); request[8] = 1; n = sizeof response;
  assert(!mulib_virtio_guest_rtc_request(&f.guest, 0, request, 16, response, &n) && response[0] == 3);
  request[8] = 0; n = 1;
  assert(!mulib_virtio_guest_rtc_request(&f.guest, 0, request, 16, response, &n) && response[0] == 4 && n == 1);
  request[2] = 1; n = sizeof response;
  assert(!mulib_virtio_guest_rtc_request(&f.guest, 0, request, 16, response, &n) && response[0] == 4);
}

static void request_entry_points(void)
{
  typedef int (*request_fn)(mulib_virtio_guest_t *, unsigned, const void *, size_t, void *, size_t *);
  static const request_fn functions[] = {
    mulib_virtio_guest_net_request, mulib_virtio_guest_blk_request,
    mulib_virtio_guest_console_request, mulib_virtio_guest_rng_request,
    mulib_virtio_guest_balloon_request, mulib_virtio_guest_rpmsg_request,
    mulib_virtio_guest_scsi_request, mulib_virtio_guest_p9_request,
    mulib_virtio_guest_gpu_request, mulib_virtio_guest_rtc_request,
    mulib_virtio_guest_input_request, mulib_virtio_guest_vsock_request,
    mulib_virtio_guest_crypto_request, mulib_virtio_guest_iommu_request,
    mulib_virtio_guest_mem_request, mulib_virtio_guest_snd_request,
    mulib_virtio_guest_fs_request, mulib_virtio_guest_pmem_request,
    mulib_virtio_guest_rpmb_request, mulib_virtio_guest_scmi_request,
    mulib_virtio_guest_i2c_request, mulib_virtio_guest_can_request,
    mulib_virtio_guest_bt_request, mulib_virtio_guest_gpio_request,
    mulib_virtio_guest_spi_request, mulib_virtio_guest_nsm_request
  };
  mulib_virtio_guest_t g = {0};
  size_t n = 0;
  for (size_t i = 0; i < sizeof functions / sizeof functions[0]; ++i) {
    assert(functions[i](NULL, 0, NULL, 0, NULL, &n) == MULIB_VMMDEV_EINVAL);
    assert(functions[i](&g, 0, NULL, 0, NULL, &n) == MULIB_VMMDEV_EINVAL);
  }
}

static void nsm_device(void)
{
  fixture_t f;
  unsigned char request[] = {0x69,'G','e','t','R','a','n','d','o','m'};
  unsigned char response[128], bad[32];
  size_t n;
  unsigned calls;
  init(&f, MULIB_VIRTIO_NSM, NULL, 0, 0);
  n = sizeof response;
  assert(!mulib_virtio_guest_nsm_request(&f.guest, 0, request, sizeof request, response, &n)
         && n == 21 && response[20] == 0x36);
  f.short_reply = 1; n = sizeof response; memset(response, 0x77, sizeof response);
  assert(mulib_virtio_guest_nsm_request(&f.guest, 0, request, sizeof request, response, &n)
         == MULIB_VMMDEV_EIO && response[0] == 0x77);
  f.short_reply = 0; f.again = 1; n = sizeof response;
  assert(mulib_virtio_guest_nsm_request(&f.guest, 0, request, sizeof request, response, &n) == MULIB_VMMDEV_AGAIN);
  f.again = 0; calls = f.calls;
  memcpy(bad, request, sizeof request); bad[sizeof request] = 0; n = sizeof response;
  assert(mulib_virtio_guest_nsm_request(&f.guest, 0, bad, sizeof request + 1, response, &n) == MULIB_VMMDEV_EINVAL);
  memset(bad, 0x81, sizeof bad); n = sizeof response;
  assert(mulib_virtio_guest_nsm_request(&f.guest, 0, bad, sizeof bad, response, &n) == MULIB_VMMDEV_EINVAL);
  bad[0] = 0x61; bad[1] = 0xff; n = sizeof response;
  assert(mulib_virtio_guest_nsm_request(&f.guest, 0, bad, 2, response, &n) == MULIB_VMMDEV_EINVAL);
  assert(f.calls == calls);
  request[1] = 'X'; n = sizeof response;
  assert(mulib_virtio_guest_nsm_request(&f.guest, 0, request, sizeof request, response, &n) == MULIB_VMMDEV_EIO);
}

static void new_configurations(void)
{
  unsigned char input[512], output[512], config[32] = {0};
  mulib_virtio_os_options_t options = {0};
  mulib_virtio_os_device_t device, previous;
  options.instance = 1; options.input_scratch = input; options.output_scratch = output;
  options.transfer_limit = 512; options.service = service; options.config = config;
  memset(&device, 0xa5, sizeof device); memcpy(&previous, &device, sizeof device);
  options.device_id = MULIB_VIRTIO_CAN; options.config_size = 2;
  assert(mulib_virtio_os_init(&device, &options) == MULIB_VMMDEV_ENOTSUP);
  options.features = 1; config[0] = 2;
  assert(mulib_virtio_os_init(&device, &options) == MULIB_VMMDEV_EINVAL);
  assert(!memcmp(&device, &previous, sizeof device));
  options.device_id = MULIB_VIRTIO_BT; options.config_size = 5; options.features = 0;
  assert(mulib_virtio_os_init(&device, &options) == MULIB_VMMDEV_EINVAL);
  config[0] = 0; options.features = 8;
  assert(mulib_virtio_os_init(&device, &options) == MULIB_VMMDEV_ENOTSUP);
  assert(!memcmp(&device, &previous, sizeof device));
  options.device_id = MULIB_VIRTIO_SPI; options.config_size = 32; options.features = 0;
  assert(mulib_virtio_os_init(&device, &options) == MULIB_VMMDEV_EINVAL);
  config[0] = 1; config[8] = 5; config[2] = 8;
  assert(mulib_virtio_os_init(&device, &options) == MULIB_VMMDEV_EINVAL);
  config[2] = 0; config[8] = 1;
  assert(mulib_virtio_os_init(&device, &options) == MULIB_VMMDEV_EINVAL);
  assert(!memcmp(&device, &previous, sizeof device));
  options.device_id = MULIB_VIRTIO_RNG; options.config_size = 0;
  options.input_scratch = (unsigned char *)&device;
  assert(mulib_virtio_os_init(&device, &options) == MULIB_VMMDEV_EINVAL);
  assert(!memcmp(&device, &previous, sizeof device));
  options.input_scratch = input; options.output_scratch = (unsigned char *)&device;
  assert(mulib_virtio_os_init(&device, &options) == MULIB_VMMDEV_EINVAL);
  assert(!memcmp(&device, &previous, sizeof device));
}

int main(void)
{
  assert(mulib_virtio_profile_count() == 48);
  for (unsigned id = 0; id <= 51; ++id) {
    const mulib_virtio_profile_t *p = mulib_virtio_profile(id);
    assert((p != NULL) == (id >= 1 && id <= 50 && id != 14 && id != 15));
    if (p) assert(p->id == id && p->name && p->support >= 1 && p->support <= 3);
    if (p && p->support != MULIB_VIRTIO_PROFILE) {
      unsigned char input[512], output[512];
      mulib_virtio_os_options_t options;
      mulib_virtio_os_device_t device;
      memset(&options, 0, sizeof options); options.instance = 1; options.device_id = id;
      options.input_scratch = input; options.output_scratch = output; options.transfer_limit = 512; options.service = service;
      assert(mulib_virtio_os_init(&device, &options) == MULIB_VMMDEV_ENOTSUP);
    }
  }
  assert(!mulib_virtio_profile_at(48));
  transport(); block(); streams(); devices(); memory_device();
  scsi_iommu_sound(); crypto_rpmb(); gpu_vsock();
  can_device(); bt_device(); spi_device(); rtc_device(); nsm_device(); request_entry_points();
  new_configurations(); isolation(); reset_cleanup(); frontend_validation();
  return 0;
}
