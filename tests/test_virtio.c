#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_VMMDEV
#define MULIB_IMPL_VIRTIO
#define MULIB_IMPL_VIRTIO_OS
#define MULIB_IMPL_VIRTIO_GUEST
#include "mulib-virtio-os.h"
#include "mulib-virtio-guest.h"
#include "virtio.h"
#include <assert.h>
#include <string.h>

typedef struct fixture {
  unsigned char ram[8192], input[4096], output[4096], disk[4096], packet[512];
  size_t packet_size;
  unsigned calls, fail, again, direction, value;
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
  if (r->operation == MULIB_VIRTIO_SERVICE_RESET) return 0;
  if (f->again) return MULIB_VMMDEV_AGAIN;
  if (f->fail) return MULIB_VMMDEV_EIO;
  switch (r->operation) {
  case MULIB_VIRTIO_SERVICE_ENTROPY:
    memset(r->output, 0x5a, r->output_capacity); *used = r->output_capacity; break;
  case MULIB_VIRTIO_SERVICE_READ:
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
    if (r->device_id == MULIB_VIRTIO_9P) {
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
  return 0;
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
  return 0;
}
