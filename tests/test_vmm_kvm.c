#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_VMMDEV
#define MULIB_IMPL_VIRTIO
#define MULIB_IMPL_VIRTIO_OS
#define MULIB_IMPL_VMM_MEMORY
#define MULIB_IMPL_VMM
#define MULIB_IMPL_KVM
#define MULIB_IMPL_VMM_KVM
#define MULIB_IMPL_VMM_HOST
#include "mulib-vmm-kvm.h"
#include "mulib-vmm-host.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_call(unsigned char *ram, unsigned index, unsigned op, uint32_t epoch,
                       uint64_t arg, const unsigned char *spans, unsigned count)
{
  unsigned char *code = ram + 0x1003 + 8 * index, *frame = ram + 0x2000 + 32 * index;
  mulib_vmmdev_message_t m = {0};
  size_t n = 128;
  code[0] = 0x66; code[1] = 0xb8; mulib_endian_store_le32(code + 2, 0x2000 + 32 * index);
  code[6] = 0x66; code[7] = 0xef;
  m.opcode = (uint16_t)op; m.device = 1; m.epoch = epoch; m.token = index + 1; m.argument = arg;
  m.count = count; m.length = 16 * count; m.payload = spans;
  assert(!mulib_vmmdev_encode(&m, ram + 0x3000 + 128 * index, &n));
  mulib_vmmdev_store64(frame, 0x3000 + 128 * index); mulib_vmmdev_store64(frame + 8, 0x4000 + 128 * index);
  mulib_endian_store_le32(frame + 16, (uint32_t)n); mulib_endian_store_le32(frame + 20, 128);
  mulib_endian_store_le32(frame + 24, UINT32_MAX);
}
int main(void)
{
  mulib_vmm_t vm = MULIB_VMM_INIT;
  mulib_vmm_kvm_t kvm = MULIB_VMM_KVM_INIT;
  mulib_vmm_host_block_t disk;
  mulib_virtio_os_device_t device;
  mulib_virtio_os_options_t d;
  mulib_vmm_options_t o;
  mulib_vmm_binding_t binding;
  mulib_vmmdev_region_t region, dma;
  unsigned char *ram, media[512] = {0}, input[1024], output[1024], config[8], spans[48], inventory[24];
  mulib_vmm_exit_t exit;
  size_t invsize = sizeof inventory;
  int rc;
  /* C11 aligned_alloc is used only by the test; the headers own no RAM allocation. */
  ram = (unsigned char *)aligned_alloc(65536, 65536); assert(ram); memset(ram, 0, 65536);
  memset(&d, 0, sizeof d); memset(&o, 0, sizeof o);
  assert(!mulib_vmm_host_block_init(&disk, media, sizeof media, 0, NULL));
  assert(!mulib_vmm_host_block_config(&disk, config, &d.features));
  d.instance = 1; d.device_id = 2; d.config = config; d.config_size = 8;
  d.transfer_limit = 1024; d.input_scratch = input; d.output_scratch = output;
  d.service = mulib_vmm_host_block_service; d.context = &disk;
  assert(!mulib_virtio_os_init(&device, &d));
  region.address = 0; region.size = 65536; region.data = ram; region.permissions = 3;
  dma.address = 0x6000; dma.size = 4096; dma.data = ram + 0x6000; dma.permissions = 3;
  binding.device = &device; binding.dma.regions = &dma; binding.dma.count = 1;
  o.ram.regions = &region; o.ram.count = 1; o.devices = &binding; o.device_count = 1;
  o.executor = mulib_vmm_kvm_executor(&kvm);
  assert(!mulib_vmm_init(&vm, &o));
  assert(!mulib_vmm_inventory(&vm, inventory, &invsize));
  assert(!mulib_vmm_load(&vm, 0x500, inventory, invsize));
  ram[0x1000] = 0xba; ram[0x1001] = 0x58; ram[0x1002] = 0x56;
  emit_call(ram, 0, MULIB_VMMDEV_PROBE, 0, 0, NULL, 0);
  emit_call(ram, 1, MULIB_VMMDEV_RESET, 1, 0, NULL, 0);
  emit_call(ram, 2, MULIB_VMMDEV_STATUS, 2, 1, NULL, 0);
  emit_call(ram, 3, MULIB_VMMDEV_STATUS, 2, 3, NULL, 0);
  emit_call(ram, 4, MULIB_VMMDEV_FEATURES, 2, MULIB_VIRTIO_VERSION_1, NULL, 0);
  emit_call(ram, 5, MULIB_VMMDEV_STATUS, 2, 11, NULL, 0);
  emit_call(ram, 6, MULIB_VMMDEV_STATUS, 2, 15, NULL, 0);
  mulib_virtio_blk_header(ram + 0x6000, 1, 0); memset(ram + 0x6100, 0x9b, 512); ram[0x6300] = 0xff;
  mulib_vmmdev_store64(spans, 0x6000); mulib_endian_store_le32(spans + 8, 16); mulib_endian_store_le32(spans + 12, 0);
  mulib_vmmdev_store64(spans + 16, 0x6100); mulib_endian_store_le32(spans + 24, 512); mulib_endian_store_le32(spans + 28, 0);
  mulib_vmmdev_store64(spans + 32, 0x6300); mulib_endian_store_le32(spans + 40, 1); mulib_endian_store_le32(spans + 44, 1);
  emit_call(ram, 7, MULIB_VMMDEV_SUBMIT, 2, 0, spans, 3);
  mulib_virtio_blk_header(ram + 0x6400, 0, 0); ram[0x6700] = 0xff;
  mulib_vmmdev_store64(spans, 0x6400); mulib_vmmdev_store64(spans + 16, 0x6500);
  mulib_endian_store_le32(spans + 28, 1); mulib_vmmdev_store64(spans + 32, 0x6700);
  emit_call(ram, 8, MULIB_VMMDEV_SUBMIT, 2, 0, spans, 3); ram[0x1003 + 8 * 9] = 0xf4;
  rc = mulib_vmm_kvm_open(&kvm, vm.ram, 0x1000, 0xfff0, 0x500);
  if (rc == MULIB_VMMDEV_ENOTSUP || rc == MULIB_VMMDEV_EACCESS) {
    printf("SKIP: KVM unavailable (result=%d errno=%d)\n", rc, kvm.system_error);
    assert(!mulib_vmm_close(&vm)); assert(!mulib_vmm_kvm_close(&kvm)); free(ram);
    return getenv("MULIB_REQUIRE_KVM") ? 1 : 0;
  }
  if (rc) fprintf(stderr, "KVM open: result=%d errno=%d\n", rc, kvm.system_error);
  assert(!rc && !mulib_vmm_start(&vm));
  for (unsigned i = 0; i < 10; ++i) {
    rc = mulib_vmm_step(&vm, &exit);
    if (rc) fprintf(stderr, "KVM step %u: result=%d native=%u errno=%d\n", i, rc, exit.native_reason, kvm.system_error);
    assert(!rc && exit.kind == (i == 9 ? MULIB_VMM_EXIT_HALT : MULIB_VMM_EXIT_CALL));
    if (i < 9) {
      mulib_vmmdev_message_t reply;
      unsigned char *frame = ram + 0x2000 + 32 * i;
      assert(!mulib_endian_load_le32(frame + 24));
      assert(!mulib_vmmdev_decode(ram + 0x4000 + 128 * i, mulib_endian_load_le32(frame + 28), &reply));
      assert(!reply.argument && reply.token == i + 1);
    }
  }
  assert(vm.state == MULIB_VMM_HALTED && vm.dispatches == 9);
  assert(!memcmp(ram + 0x6500, media, 512) && media[0] == 0x9b && !ram[0x6300] && !ram[0x6700]);
  /* Reset while a new I/O exit awaits KVM completion: never replay the trap. */
  assert(!mulib_vmm_reset(&vm)); assert(!mulib_vmm_start(&vm));
  assert(!mulib_vmm_step(&vm, &exit) && exit.kind == MULIB_VMM_EXIT_CALL);
  assert(kvm.pending_io && !mulib_vmm_reset(&vm) && !kvm.pending_io);
  assert(!mulib_vmm_start(&vm));
  assert(!mulib_vmm_step(&vm, &exit) && exit.kind == MULIB_VMM_EXIT_CALL);
  assert(!mulib_vmm_close(&vm)); assert(!mulib_vmm_kvm_close(&kvm)); assert(!mulib_vmm_kvm_close(&kvm));
  free(ram); puts("KVM: native guest negotiated Virtio, wrote/read a block, halted and reset successfully");
  return 0;
}
