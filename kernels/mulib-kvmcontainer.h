/** @file mulib-kvmcontainer.h
 * @brief Owning Linux KVM device/VM/single-vCPU bundle, not a guest OS
 * runtime. Define MULIB_IMPL_KVMCONTAINER and MULIB_IMPL_KVM once. */
#ifndef MULIB_KVMCONTAINER_H
#define MULIB_KVMCONTAINER_H
#include "mulib-kvm.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_kvmcontainer
  {
    mulib_kvm_t device;
    mulib_kvm_vm_t vm;
    mulib_kvm_vcpu_t cpu;
  } mulib_kvmcontainer_t;
  /** @brief Initialize fresh bundle with invalid fds. Do not copy live
   * bundles. */
  void mulib_kvmcontainer_init (mulib_kvmcontainer_t *container);
  /** @brief Open device, create native VM and vCPU 0; unwind on failure.
   * Register RAM and configure guest registers/devices through native KVM
   * APIs. 0 success, KVM error otherwise; no /dev/kvm availability is assumed.
   */
  int mulib_kvmcontainer_open (mulib_kvmcontainer_t *container,
                               const char *device_path);
  /** @brief Close CPU then VM then device; stop on failure to preserve
   * ownership. */
  int mulib_kvmcontainer_close (mulib_kvmcontainer_t *container);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_KVMCONTAINER)                                          \
    && !defined(MULIB_KVMCONTAINER_IMPLEMENTED)
#define MULIB_KVMCONTAINER_IMPLEMENTED
void
mulib_kvmcontainer_init (mulib_kvmcontainer_t *c)
{
  if (c)
    {
      c->device.fd = -1;
      c->vm.fd = -1;
      c->cpu.fd = -1;
      c->cpu.run = NULL;
      c->cpu.run_size = 0;
    }
}
int
mulib_kvmcontainer_close (mulib_kvmcontainer_t *c)
{
  int rc;
  if (!c)
    return MULIB_KVM_EINVAL;
  rc = mulib_kvm_vcpu_close (&c->cpu);
  if (rc)
    return rc;
  rc = mulib_kvm_vm_close (&c->vm);
  if (rc)
    return rc;
  return mulib_kvm_close (&c->device);
}
int
mulib_kvmcontainer_open (mulib_kvmcontainer_t *c, const char *path)
{
  int rc;
  if (!c)
    return MULIB_KVM_EINVAL;
  if (c->device.fd != -1 || c->vm.fd != -1 || c->cpu.fd != -1)
    return MULIB_KVM_ESTATE;
  rc = mulib_kvm_open (&c->device, path);
  if (!rc)
    rc = mulib_kvm_vm_create (&c->device, &c->vm);
  if (!rc)
    rc = mulib_kvm_vcpu_create (&c->device, &c->vm, 0, &c->cpu);
  if (rc)
    (void)mulib_kvmcontainer_close (c);
  return rc;
}
#endif
