# kvmcontainer

Linux KVM ownership bundle: device, native VM, vCPU 0. Call `init` on fresh
storage (zero alone is not the invalid-fd initializer). `open` performs sequential
construction, unwinding on failure. `close` releases CPU, VM, device in order,
stopping on errors to preserve resources. Returns existing KVM result codes;
no /dev/kvm or platform support is assumed. Do not copy live bundles.

Guest RAM mapping, architecture registers, devices, interrupts, guest boot,
isolation policy and execution loop remain caller-owned. Not an OCI engine.
Switches: `MULIB_IMPL_KVMCONTAINER`, `MULIB_IMPL_KVM`.
Example: `examples/modules/kvmcontainer.c` is hardware-free.
