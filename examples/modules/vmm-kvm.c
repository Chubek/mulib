#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_VMMDEV
#define MULIB_IMPL_VIRTIO
#define MULIB_IMPL_VIRTIO_OS
#define MULIB_IMPL_VMM_MEMORY
#define MULIB_IMPL_VMM
#define MULIB_IMPL_KVM
#define MULIB_IMPL_VMM_KVM
#include "mulib-vmm-kvm.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
  mulib_vmm_t vm = MULIB_VMM_INIT;
  mulib_vmm_kvm_t kvm = MULIB_VMM_KVM_INIT;
  mulib_vmm_options_t options;
  mulib_vmmdev_region_t region;
  mulib_vmm_exit_t exit;
  unsigned char *ram = (unsigned char *)aligned_alloc(65536, 65536);
  unsigned char inventory[16], code[] = {0xf4}; /* HLT, no firmware or guest kernel. */
  size_t n = sizeof inventory;
  int rc;
  assert(ram); memset(ram, 0, 65536); memset(&options, 0, sizeof options);
  region.address = 0; region.size = 65536; region.data = ram; region.permissions = 3;
  options.ram.regions = &region; options.ram.count = 1; options.executor = mulib_vmm_kvm_executor(&kvm);
  assert(!mulib_vmm_init(&vm, &options));
  assert(!mulib_vmm_inventory(&vm, inventory, &n));
  assert(!mulib_vmm_load(&vm, 0x500, inventory, n));
  assert(!mulib_vmm_load(&vm, 0x1000, code, sizeof code));
  rc = mulib_vmm_kvm_open(&kvm, vm.ram, 0x1000, 0xfff0, 0x500);
  if (rc == MULIB_VMMDEV_ENOTSUP || rc == MULIB_VMMDEV_EACCESS)
    printf("SKIP: KVM unavailable (result=%d errno=%d)\n", rc, kvm.system_error);
  else {
    assert(!rc); assert(!mulib_vmm_start(&vm));
    assert(!mulib_vmm_step(&vm, &exit) && exit.kind == MULIB_VMM_EXIT_HALT);
    puts("MuLIB VMM: native x86 guest halted successfully");
  }
  assert(!mulib_vmm_close(&vm)); assert(!mulib_vmm_kvm_close(&kvm)); free(ram);
  return 0;
}
