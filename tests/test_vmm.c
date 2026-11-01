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

typedef struct fixture {
  mulib_vmm_t vm;
  mulib_virtio_os_device_t device;
  mulib_vmm_host_block_t disk;
  unsigned char ram[8192], media[1024], input[2048], output[2048];
  unsigned locked, locks, unlocks, calls, resets, fail_reset, lock_fail, executed;
} fixture_t;
static int lock_vm(void *context)
{
  fixture_t *f = (fixture_t *)context;
  assert(!f->locked);
  if (f->lock_fail) return MULIB_VMMDEV_AGAIN;
  f->locked = 1; ++f->locks; return 0;
}
static void unlock_vm(void *context)
{
  fixture_t *f = (fixture_t *)context;
  assert(f->locked); f->locked = 0; ++f->unlocks;
}
static int block_service(void *context, const mulib_virtio_service_request_t *r, size_t *used)
{
  fixture_t *f = (fixture_t *)context;
  assert(f->locked); ++f->calls;
  if (r->operation == MULIB_VIRTIO_SERVICE_RESET) {
    ++f->resets;
    if (f->fail_reset) return MULIB_VMMDEV_EIO;
  }
  return mulib_vmm_host_block_service(&f->disk, r, used);
}
static int cpu_step(void *context, mulib_vmm_exit_t *e)
{
  fixture_t *f = (fixture_t *)context;
  assert(f->locked);
  if (!f->executed++) { e->kind = MULIB_VMM_EXIT_CALL; e->address = 0x1000; }
  else e->kind = MULIB_VMM_EXIT_HALT;
  return 0;
}
static int cpu_reset(void *context)
{
  fixture_t *f = (fixture_t *)context;
  assert(f->locked); f->executed = 0; return 0;
}
static void init(fixture_t *f)
{
  mulib_vmm_options_t o;
  mulib_virtio_os_options_t d;
  unsigned char config[8];
  mulib_vmmdev_region_t ram, dma;
  mulib_vmm_binding_t binding;
  memset(f, 0, sizeof *f); memset(&o, 0, sizeof o); memset(&d, 0, sizeof d);
  assert(!mulib_vmm_host_block_init(&f->disk, f->media, sizeof f->media, 0, NULL));
  assert(!mulib_vmm_host_block_config(&f->disk, config, &d.features));
  d.instance = 7; d.device_id = MULIB_VIRTIO_BLK; d.config = config; d.config_size = 8;
  d.input_scratch = f->input; d.output_scratch = f->output; d.transfer_limit = sizeof f->input;
  d.service = block_service; d.context = f;
  assert(!mulib_virtio_os_init(&f->device, &d));
  ram.address = 0x1000; ram.size = sizeof f->ram; ram.data = f->ram; ram.permissions = 3;
  dma.address = 0x2000; dma.size = 2048; dma.data = f->ram + 4096; dma.permissions = 3;
  binding.device = &f->device; binding.dma.regions = &dma; binding.dma.count = 1;
  o.ram.regions = &ram; o.ram.count = 1; o.devices = &binding; o.device_count = 1;
  o.lock = lock_vm; o.unlock = unlock_vm; o.lock_context = f;
  o.executor.step = cpu_step; o.executor.reset = cpu_reset; o.executor.context = f;
  assert(!mulib_vmm_init(&f->vm, &o));
  /* Inventories above were automatic objects; the VM must have copied metadata. */
  assert(f->vm.ram.regions == f->vm.regions);
}
static void packet(fixture_t *f, unsigned op, uint32_t epoch, uint64_t arg)
{
  mulib_vmmdev_message_t m = {0};
  size_t n = 128;
  m.opcode = (uint16_t)op; m.device = 7; m.epoch = epoch; m.argument = arg; m.token = 73;
  assert(!mulib_vmmdev_encode(&m, f->ram + 128, &n));
  mulib_vmmdev_store64(f->ram, 0x1080); mulib_vmmdev_store64(f->ram + 8, 0x1200);
  mulib_endian_store_le32(f->ram + 16, (uint32_t)n); mulib_endian_store_le32(f->ram + 20, 128);
}
static void memory_tests(void)
{
  unsigned char a[16], b[16], copy[32], input[32];
  mulib_vmmdev_region_t regions[2] = {{0x1000,16,a,3},{0x1010,16,b,1}};
  mulib_vmmdev_memory_t m = {regions, 2};
  mulib_vmmdev_region_t sub = {0x1008,8,a + 8,1};
  mulib_vmmdev_memory_t view = {&sub, 1};
  memset(a, 0x11, 16); memset(b, 0x22, 16); memset(input, 0x33, 32);
  assert(!mulib_vmm_memory_read(&m, 0x1000, copy, 32) && copy[0] == 0x11 && copy[31] == 0x22);
  assert(mulib_vmm_memory_write(&m, 0x1000, input, 32) == MULIB_VMMDEV_EACCESS && a[0] == 0x11);
  regions[1].permissions = 3;
  assert(!mulib_vmm_memory_write(&m, 0x1000, input, 32) && a[0] == 0x33 && b[15] == 0x33);
  assert(!mulib_vmm_memory_zero(&m, 0x1008, 16) && a[7] == 0x33 && !a[8] && !b[7] && b[8] == 0x33);
  assert(!mulib_vmm_memory_subset(&m, &view));
  sub.data = b; assert(mulib_vmm_memory_subset(&m, &view) == MULIB_VMMDEV_EACCESS);
  assert(mulib_vmm_memory_write(&m, 0x1000, a, 16) == MULIB_VMMDEV_EINVAL);
  regions[1].address = 0x1020;
  assert(mulib_vmm_memory_zero(&m, 0x1000, 32) == MULIB_VMMDEV_EBOUNDS && a[0] == 0x33);
  assert(mulib_vmm_memory_check(&m, UINT64_MAX, 1, 1) == MULIB_VMMDEV_EINVAL);
  assert(!mulib_vmm_memory_check(&m, UINT64_MAX, 0, 1));
}
static void construction_tests(void)
{
  fixture_t f;
  mulib_vmm_t other = MULIB_VMM_INIT;
  mulib_vmm_options_t options;
  mulib_vmm_binding_t bindings[2];
  mulib_vmmdev_region_t dma;
  init(&f); memset(&options, 0, sizeof options);
  options.ram = f.vm.ram; options.devices = bindings; options.device_count = 2;
  dma = f.vm.slots[0].regions[0];
  bindings[0].device = &f.device; bindings[0].dma.regions = &dma; bindings[0].dma.count = 1;
  bindings[1] = bindings[0];
  assert(mulib_vmm_init(&other, &options) == MULIB_VMMDEV_EINVAL && other.state == MULIB_VMM_EMPTY);
  options.device_count = 1; dma.data = f.ram;
  assert(mulib_vmm_init(&other, &options) == MULIB_VMMDEV_EACCESS && !other.self);
  dma = f.vm.slots[0].regions[0]; options.lock = lock_vm;
  assert(mulib_vmm_init(&other, &options) == MULIB_VMMDEV_EINVAL);
  options.lock = NULL; options.ram.count = MULIB_VMM_MAX_REGIONS + 1;
  assert(mulib_vmm_init(&other, &options) == MULIB_VMMDEV_EINVAL);
  assert(!mulib_vmm_close(&f.vm));
}
static void host_tests(void)
{
  unsigned char media[512] = {0}, data[512], config[8];
  mulib_vmm_host_block_t disk;
  mulib_virtio_service_request_t request;
  uint64_t features;
  size_t used = 999;
  assert(mulib_vmm_host_block_init(&disk, media, 511, 0, NULL) == MULIB_VMMDEV_EINVAL);
  assert(!mulib_vmm_host_block_init(&disk, media, 512, 1, NULL));
  assert(!mulib_vmm_host_block_config(&disk, config, &features) && mulib_vmmdev_load64(config) == 1);
  assert(features & (UINT64_C(1) << 5));
  memset(&request, 0, sizeof request); request.device_id = 2; request.operation = MULIB_VIRTIO_SERVICE_WRITE;
  request.input = data; request.input_size = sizeof data;
  assert(mulib_vmm_host_block_service(&disk, &request, &used) == MULIB_VMMDEV_EACCESS && !used);
  request.operation = MULIB_VIRTIO_SERVICE_READ; request.output = data; request.output_capacity = sizeof data;
  request.address = UINT64_MAX;
  assert(mulib_vmm_host_block_service(&disk, &request, &used) == MULIB_VMMDEV_EBOUNDS);
  request.address = 0;
  assert(!mulib_vmm_host_block_service(&disk, &request, &used) && used == 512 && !data[0]);
  request.operation = MULIB_VIRTIO_SERVICE_ENTROPY; request.device_id = 4; request.input_size = 0;
  {
    int rc = mulib_vmm_host_entropy_service(NULL, &request, &used);
    assert(rc == 0 || rc == MULIB_VMMDEV_AGAIN || rc == MULIB_VMMDEV_ENOTSUP);
    if (!rc) assert(used > 0 && used <= sizeof data);
  }
}
static void frontend_tests(fixture_t *f)
{
  mulib_virtio_guest_t guest;
  unsigned char written[512], readback[512];
  mulib_vmmdev_span_t denied = {0x1800, 16, 0};
  size_t used;
  unsigned calls;
  assert(!mulib_virtio_guest_attach(&guest, mulib_vmm_exchange, &f->vm, 7, UINT64_C(1) << 9,
                                   0x2000, f->ram + 4096, 2048));
  assert(!mulib_virtio_guest_start(&guest));
  memset(written, 0x59, sizeof written);
  assert(!mulib_virtio_guest_block(&guest, 1, 1, written, sizeof written));
  assert(!mulib_virtio_guest_block(&guest, 0, 1, readback, sizeof readback));
  assert(!memcmp(written, readback, 512));
  calls = f->calls;
  assert(mulib_virtio_guest_submit(&guest, 0, &denied, 1, &used) == MULIB_VMMDEV_EBOUNDS);
  assert(f->calls == calls); /* CPU-visible RAM outside this device's allowlist is denied. */
  assert(!mulib_vmm_reset(&f->vm));
  assert(mulib_virtio_guest_block(&guest, 0, 0, readback, sizeof readback) == MULIB_VMMDEV_ESTALE);
  assert(f->media[512] == 0x59);
}
static void trap_tests(fixture_t *f)
{
  mulib_vmmdev_message_t r;
  unsigned calls;
  size_t cap;
  packet(f, MULIB_VMMDEV_PROBE, 0, 0);
  assert(!mulib_vmm_trap(&f->vm, 0x1000));
  assert(!mulib_endian_load_le32(f->ram + 24) && mulib_endian_load_le32(f->ram + 28) == 72);
  assert(!mulib_vmmdev_decode(f->ram + 512, 72, &r) && r.token == 73 && !r.argument);
  packet(f, MULIB_VMMDEV_RESET, f->device.epoch, 0); calls = f->calls;
  mulib_vmmdev_store64(f->ram + 8, 0xdead0000);
  assert(!mulib_vmm_trap(&f->vm, 0x1000));
  assert(mulib_endian_load_le32(f->ram + 24) == MULIB_VMMDEV_EBOUNDS && f->calls == calls);
  packet(f, MULIB_VMMDEV_RESET, f->device.epoch, 0);
  mulib_vmmdev_store64(f->ram + 8, 0x1088);
  assert(!mulib_vmm_trap(&f->vm, 0x1000));
  assert(mulib_endian_load_le32(f->ram + 24) == MULIB_VMMDEV_EINVAL && f->calls == calls);
  packet(f, MULIB_VMMDEV_RESET, f->device.epoch, 0);
  mulib_endian_store_le32(f->ram + 20, 39);
  assert(!mulib_vmm_trap(&f->vm, 0x1000));
  assert(mulib_endian_load_le32(f->ram + 24) == MULIB_VMMDEV_ENOSPC && f->calls == calls);
  assert(mulib_endian_load_le32(f->ram + 28) == 40);
  assert(mulib_vmm_trap(&f->vm, UINT64_MAX - 15) == MULIB_VMMDEV_EINVAL);
  packet(f, MULIB_VMMDEV_PROBE, 0, 0); cap = 40;
  assert(mulib_vmm_call(&f->vm, 0x1080, 40, 0x1200, &cap) == MULIB_VMMDEV_ENOSPC && cap == 72);
  /* SUBMIT metadata is snapshotted and forbidden from aliasing the mailbox. */
  {
    mulib_vmmdev_message_t m = {0}; unsigned char span[16]; size_t n = 128;
    mulib_vmmdev_store64(span, 0x1000); mulib_endian_store_le32(span + 8, 32); mulib_endian_store_le32(span + 12, 1);
    m.opcode = MULIB_VMMDEV_SUBMIT; m.device = 7; m.epoch = f->device.epoch;
    m.count = 1; m.length = 16; m.payload = span;
    assert(!mulib_vmmdev_encode(&m, f->ram + 128, &n));
    mulib_endian_store_le32(f->ram + 16, (uint32_t)n); mulib_endian_store_le32(f->ram + 20, 128);
    assert(!mulib_vmm_trap(&f->vm, 0x1000));
    assert(mulib_endian_load_le32(f->ram + 24) == MULIB_VMMDEV_EINVAL && f->calls == calls);
  }
}
int main(void)
{
  fixture_t f;
  mulib_vmm_exit_t exit;
  unsigned char inventory[MULIB_VMM_INVENTORY_MAX];
  size_t n;
  memory_tests(); construction_tests(); host_tests(); init(&f);
  n = 1; memset(inventory, 0xcc, sizeof inventory);
  assert(mulib_vmm_inventory(&f.vm, inventory, &n) == MULIB_VMMDEV_ENOSPC && n == 24 && inventory[0] == 0xcc);
  n = sizeof inventory; assert(!mulib_vmm_inventory(&f.vm, inventory, &n) && n == 24);
  assert(!memcmp(inventory, "MVID\1\0\0\0\1\0\0\0\30\0\0\0", 16));
  assert(mulib_endian_load_le32(inventory + 16) == 7 && mulib_endian_load_le32(inventory + 20) == 2);
  assert(!mulib_vmm_load(&f.vm, 0x1f00, inventory, n));
  frontend_tests(&f); trap_tests(&f);
  packet(&f, MULIB_VMMDEV_PROBE, 0, 0);
  assert(!mulib_vmm_start(&f.vm));
  assert(mulib_vmm_load(&f.vm, 0x1f00, inventory, n) == MULIB_VMMDEV_ESTATE);
  assert(!mulib_vmm_step(&f.vm, &exit) && exit.kind == MULIB_VMM_EXIT_CALL);
  assert(!mulib_vmm_stop(&f.vm) && !mulib_vmm_start(&f.vm));
  assert(!mulib_vmm_step(&f.vm, &exit) && exit.kind == MULIB_VMM_EXIT_HALT && f.vm.state == MULIB_VMM_HALTED);
  assert(mulib_vmm_start(&f.vm) == MULIB_VMMDEV_ESTATE);
  assert(!mulib_vmm_reset(&f.vm) && f.vm.state == MULIB_VMM_READY);
  f.fail_reset = 1;
  assert(mulib_vmm_reset(&f.vm) == MULIB_VMMDEV_EIO && f.vm.state == MULIB_VMM_FAULTED);
  assert(mulib_vmm_start(&f.vm) == MULIB_VMMDEV_ESTATE);
  f.fail_reset = 0; assert(!mulib_vmm_reset(&f.vm));
  f.lock_fail = 1; assert(mulib_vmm_start(&f.vm) == MULIB_VMMDEV_AGAIN); f.lock_fail = 0;
  assert(f.locks == f.unlocks && !f.locked);
  f.device.epoch = UINT32_MAX; /* An exhausted instance can still be retired securely. */
  assert(mulib_vmm_reset(&f.vm) == MULIB_VMMDEV_ESTATE);
  f.device.sessions[0].key[0] = 0x55;
  assert(!mulib_vmm_close(&f.vm) && !mulib_vmm_close(&f.vm));
  assert(!f.device.epoch && !f.device.sessions[0].key[0]);
  assert(mulib_vmm_start(&f.vm) == MULIB_VMMDEV_ESTATE);
  return 0;
}
