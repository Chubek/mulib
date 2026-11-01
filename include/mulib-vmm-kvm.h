/**
 * @file mulib-vmm-kvm.h
 * @brief Linux x86 KVM execution adapter and MuLIB VMM guest-call port binding.
 * Define MULIB_IMPL_VMM_KVM, MULIB_IMPL_KVM and the VMM dependency switches once.
 * One vCPU, real-mode entry, caller-owned page-aligned RAM; no firmware required.
 */
#ifndef MULIB_VMM_KVM_H
#define MULIB_VMM_KVM_H
#include "mulib-vmm.h"
#include "mulib-kvm.h"
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_VMM_KVM_PORT 0x5658u
/** @brief Owning KVM descriptors, borrowed RAM. Initialize with MULIB_VMM_KVM_INIT. */
typedef struct mulib_vmm_kvm {
  mulib_kvm_t device;
  mulib_kvm_vm_t machine;
  mulib_kvm_vcpu_t cpu;
  unsigned opened, pending_io;
  uint16_t entry, stack;
  uint32_t inventory_address;
  int system_error;
} mulib_vmm_kvm_t;
#define MULIB_VMM_KVM_INIT {MULIB_KVM_INIT, MULIB_KVM_VM_INIT, MULIB_KVM_VCPU_INIT, 0, 0, 0, 0, 0, 0}
/**
 * @brief Open one KVM vCPU using validated RAM; CPU starts at 0000:entry, SP=stack.
 * ESI contains inventory_address (host supplies MVID there). RAM stays alive until close.
 * Entry/stack must map below 64KiB. CPU RAM must be page aligned and below 0xfffbd000.
 * On failure descriptors are closed; system_error records native errno where relevant.
 */
int mulib_vmm_kvm_open(mulib_vmm_kvm_t *kvm, mulib_vmmdev_memory_t ram,
                       uint16_t entry, uint16_t stack, uint32_t inventory_address);
/** @brief Close native resources in vCPU/VM/device order; idempotent, no RAM free. */
int mulib_vmm_kvm_close(mulib_vmm_kvm_t *kvm);
/** @brief Return callbacks for mulib_vmm_options.executor; kvm must remain at a stable address. */
mulib_vmm_executor_t mulib_vmm_kvm_executor(mulib_vmm_kvm_t *kvm);
/** @brief Run one exit. OUT DX,EAX to 0x5658 delivers a 32-bit call-frame GPA. */
int mulib_vmm_kvm_step(void *kvm, mulib_vmm_exit_t *exit);
/** @brief Restore real-mode entry registers, completing any pending port exit first. */
int mulib_vmm_kvm_reset(void *kvm);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_VMM_KVM) && !defined(MULIB_VMM_KVM_IMPLEMENTED)
#define MULIB_VMM_KVM_IMPLEMENTED
#include <errno.h>
#include <string.h>
#if defined(__linux__) && (defined(__i386__) || defined(__x86_64__))
#include <linux/kvm.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif
#if defined(__linux__) && (defined(__i386__) || defined(__x86_64__))
static int mulib_vmm_kvm_ioctl_(int fd, unsigned long op, uintptr_t argument)
{
  int rc;
  do { rc = ioctl(fd, op, argument); } while (rc < 0 && errno == EINTR);
  return rc;
}
static void mulib_vmm_kvm_segment_(struct kvm_segment *s, int code)
{
  memset(s, 0, sizeof *s); s->limit = 0xffff; s->present = 1; s->s = 1; s->type = code ? 11 : 3;
}
#endif
int mulib_vmm_kvm_close(mulib_vmm_kvm_t *k)
{
  int rc, result = 0;
  if (!k) return MULIB_VMMDEV_EINVAL;
  rc = mulib_kvm_vcpu_close(&k->cpu);
  if (rc) { k->system_error = errno; return MULIB_VMMDEV_EIO; }
  rc = mulib_kvm_vm_close(&k->machine); if (rc) { k->system_error = errno; result = MULIB_VMMDEV_EIO; }
  rc = mulib_kvm_close(&k->device); if (rc) { k->system_error = errno; result = MULIB_VMMDEV_EIO; }
  k->opened = 0; k->pending_io = 0; return result;
}
int mulib_vmm_kvm_reset(void *context)
{
  mulib_vmm_kvm_t *k = (mulib_vmm_kvm_t *)context;
  if (!k || !k->opened) return MULIB_VMMDEV_ESTATE;
#if defined(__linux__) && (defined(__i386__) || defined(__x86_64__))
  {
    struct kvm_sregs sregs;
    struct kvm_regs regs;
    struct kvm_run *run = (struct kvm_run *)k->cpu.run;
    if (k->pending_io) {
      uint32_t reason;
      int rc;
      run->immediate_exit = 1; rc = mulib_kvm_run(&k->cpu, &reason); run->immediate_exit = 0;
      if (rc != MULIB_KVM_INTERRUPTED) { k->system_error = errno; return MULIB_VMMDEV_EIO; }
      k->pending_io = 0;
    }
    if (mulib_vmm_kvm_ioctl_(k->cpu.fd, KVM_GET_SREGS, (uintptr_t)&sregs) < 0) goto fail;
    mulib_vmm_kvm_segment_(&sregs.cs, 1); mulib_vmm_kvm_segment_(&sregs.ds, 0);
    mulib_vmm_kvm_segment_(&sregs.es, 0); mulib_vmm_kvm_segment_(&sregs.fs, 0);
    mulib_vmm_kvm_segment_(&sregs.gs, 0); mulib_vmm_kvm_segment_(&sregs.ss, 0);
    sregs.cr0 = 0x10; sregs.cr2 = sregs.cr3 = sregs.cr4 = sregs.cr8 = sregs.efer = 0;
    sregs.idt.base = 0; sregs.idt.limit = 0x3ff; sregs.gdt.base = 0; sregs.gdt.limit = 0;
    memset(sregs.interrupt_bitmap, 0, sizeof sregs.interrupt_bitmap);
    if (mulib_vmm_kvm_ioctl_(k->cpu.fd, KVM_SET_SREGS, (uintptr_t)&sregs) < 0) goto fail;
    memset(&regs, 0, sizeof regs); regs.rip = k->entry; regs.rsp = k->stack;
    regs.rsi = k->inventory_address; regs.rflags = 2;
    if (mulib_vmm_kvm_ioctl_(k->cpu.fd, KVM_SET_REGS, (uintptr_t)&regs) < 0) goto fail;
    return 0;
  fail:
    k->system_error = errno; return MULIB_VMMDEV_EIO;
  }
#else
  return MULIB_VMMDEV_ENOTSUP;
#endif
}
int mulib_vmm_kvm_open(mulib_vmm_kvm_t *k, mulib_vmmdev_memory_t ram,
                       uint16_t entry, uint16_t stack, uint32_t inventory)
{
  int rc;
  if (!k || !ram.count || ram.count > MULIB_VMM_MAX_REGIONS || stack < 2) return MULIB_VMMDEV_EINVAL;
  if (k->opened || k->device.fd != -1 || k->machine.fd != -1 || k->cpu.fd != -1 || k->cpu.run)
    return MULIB_VMMDEV_ESTATE;
  rc = mulib_vmmdev_memory_validate(&ram); if (rc) return rc;
  rc = mulib_vmm_memory_check(&ram, entry, 1, MULIB_VMMDEV_READ); if (rc) return rc;
  rc = mulib_vmm_memory_check(&ram, (uint64_t)stack - 2, 2, 3); if (rc) return rc;
  rc = mulib_vmm_memory_check(&ram, inventory, 16, MULIB_VMMDEV_READ); if (rc) return rc;
#if defined(__linux__) && (defined(__i386__) || defined(__x86_64__))
  {
    int cap, saved;
    long page = sysconf(_SC_PAGESIZE);
    if (page <= 0) return MULIB_VMMDEV_EIO;
    for (size_t i = 0; i < ram.count; ++i) {
      const mulib_vmmdev_region_t *r = ram.regions + i;
      if (r->address % (uint64_t)page || r->size % (size_t)page || (uintptr_t)r->data % (uintptr_t)page
          || !(r->permissions & MULIB_VMMDEV_READ) || r->address >= UINT64_C(0xfffbd000)
          || r->size > UINT64_C(0xfffbd000) - r->address) return MULIB_VMMDEV_EINVAL;
    }
    rc = mulib_kvm_open(&k->device, NULL);
    if (rc) {
      k->system_error = errno;
      return rc == MULIB_KVM_ENOTSUP || errno == ENOENT || errno == ENODEV || errno == ENOSYS
        ? MULIB_VMMDEV_ENOTSUP : (errno == EACCES || errno == EPERM ? MULIB_VMMDEV_EACCESS : MULIB_VMMDEV_EIO);
    }
    rc = MULIB_VMMDEV_EIO;
    if (mulib_kvm_check(&k->device, KVM_CAP_IMMEDIATE_EXIT, &cap)) goto fail;
    if (!cap) { rc = MULIB_VMMDEV_ENOTSUP; goto fail; }
    if (mulib_kvm_vm_create(&k->device, &k->machine)) goto fail;
    if (mulib_kvm_check(&k->device, KVM_CAP_SET_TSS_ADDR, &cap)) goto fail;
    if (cap && mulib_vmm_kvm_ioctl_(k->machine.fd, KVM_SET_TSS_ADDR, UINT32_C(0xfffbd000)) < 0) goto fail;
    for (size_t i = 0; i < ram.count; ++i) {
      const mulib_vmmdev_region_t *r = ram.regions + i;
      int readonly = !(r->permissions & MULIB_VMMDEV_WRITE);
      if (readonly) {
        if (mulib_kvm_check(&k->device, KVM_CAP_READONLY_MEM, &cap)) goto fail;
        if (!cap) { rc = MULIB_VMMDEV_ENOTSUP; goto fail; }
      }
      if (mulib_kvm_memory(&k->machine, (uint32_t)i, r->address, r->data, r->size, readonly)) goto fail;
    }
    if (mulib_kvm_vcpu_create(&k->device, &k->machine, 0, &k->cpu)) goto fail;
    k->entry = entry; k->stack = stack; k->inventory_address = inventory; k->opened = 1;
    rc = mulib_vmm_kvm_reset(k); if (rc) goto fail;
    k->system_error = 0; return 0;
  fail:
    saved = errno; (void)mulib_vmm_kvm_close(k); k->system_error = saved; return rc;
  }
#else
  (void)entry; (void)inventory;
  return MULIB_VMMDEV_ENOTSUP;
#endif
}
mulib_vmm_executor_t mulib_vmm_kvm_executor(mulib_vmm_kvm_t *k)
{
  mulib_vmm_executor_t e;
  e.step = mulib_vmm_kvm_step; e.reset = mulib_vmm_kvm_reset; e.context = k; return e;
}
int mulib_vmm_kvm_step(void *context, mulib_vmm_exit_t *out)
{
  mulib_vmm_kvm_t *k = (mulib_vmm_kvm_t *)context;
  if (!k || !k->opened || !out) return MULIB_VMMDEV_ESTATE;
  memset(out, 0, sizeof *out);
#if defined(__linux__) && (defined(__i386__) || defined(__x86_64__))
  {
    struct kvm_run *run = (struct kvm_run *)k->cpu.run;
    uint32_t reason = 0;
    int rc = mulib_kvm_run(&k->cpu, &reason);
    if (rc == MULIB_KVM_INTERRUPTED) { k->pending_io = 0; out->kind = MULIB_VMM_EXIT_YIELD; return 0; }
    if (rc) { k->system_error = errno; return MULIB_VMMDEV_EIO; }
    k->pending_io = 0; out->native_reason = reason;
    if (reason == KVM_EXIT_HLT) { out->kind = MULIB_VMM_EXIT_HALT; return 0; }
    if (reason == KVM_EXIT_INTR) { out->kind = MULIB_VMM_EXIT_YIELD; return 0; }
    if (reason == KVM_EXIT_IO) {
      k->pending_io = 1;
      if (run->io.direction != KVM_EXIT_IO_OUT || run->io.port != MULIB_VMM_KVM_PORT
          || run->io.size != 4 || run->io.count != 1 || run->io.data_offset > k->cpu.run_size
          || 4 > k->cpu.run_size - (size_t)run->io.data_offset) return MULIB_VMMDEV_ENOTSUP;
      out->kind = MULIB_VMM_EXIT_CALL;
      out->address = mulib_endian_load_le32((const unsigned char *)run + (size_t)run->io.data_offset);
      return 0;
    }
    return reason == KVM_EXIT_SHUTDOWN || reason == KVM_EXIT_FAIL_ENTRY || reason == KVM_EXIT_INTERNAL_ERROR
      ? MULIB_VMMDEV_EIO : MULIB_VMMDEV_ENOTSUP;
  }
#else
  return MULIB_VMMDEV_ENOTSUP;
#endif
}
#ifdef __cplusplus
}
#endif
#endif
