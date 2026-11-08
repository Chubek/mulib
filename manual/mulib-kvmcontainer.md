# kvmcontainer

[Manuals](README.md) · [Library index](index.md)

Linux KVM ownership bundle: device, native VM, vCPU 0. Call `init` on fresh
storage (zero alone is not the invalid-fd initializer). `open` performs sequential
construction, unwinding on failure. `close` releases CPU, VM, device in order,
stopping on errors to preserve resources. Returns existing KVM result codes;
no /dev/kvm or platform support is assumed. Do not copy live bundles.

Guest RAM mapping, architecture registers, devices, interrupts, guest boot,
isolation policy and execution loop remain caller-owned. Not an OCI engine.
Switches: `MULIB_IMPL_KVMCONTAINER`, `MULIB_IMPL_KVM`.
Example: `examples/modules/kvmcontainer.c` is hardware-free.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-kvmcontainer.h`](../include/mulib-kvmcontainer.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
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
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-kvmcontainer.h`](../include/mulib-kvmcontainer.h) |
| Implementation switch | `MULIB_IMPL_KVMCONTAINER` |
| Repository-local includes | [mulib-kvm](mulib-kvm.md) |
| Complete example | [`examples/modules/kvmcontainer.c`](../examples/modules/kvmcontainer.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/kvmcontainer.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
