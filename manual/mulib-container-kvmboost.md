# mulib-container-kvmboost

## NAME AND INTEGRATION

Owning single-vCPU KVM execution adapter with explicit guest and device callbacks.
Define `MULIB_IMPL_CONTAINER_KVMBOOST`, `MULIB_IMPL_KVMCONTAINER`,
`MULIB_IMPL_KVM` and `containerboost` transitive switches. Example:
`examples/modules/container-kvmboost.c`.

## GUEST CONTRACT

`init` zeroes configuration and sets one million maximum exits. Supply all three
callbacks: configure(user,bundle,spec,rootfs), reset(user,bundle), and
exit(user,bundle,native_reason,&code). Configure registers borrowed page-aligned
RAM and devices; reset establishes the initial guest CPU state; exit emulates or
handles one native KVM exit and returns AGAIN, DONE or a negative CT error.

Configuration is copied into the prepared handle. The device path is borrowed
through preparation; user callback context and RAM must remain live until close.
Callbacks may use existing `mulib_kvm_memory` and native register ioctls through
the supplied bundle. They own external device/RAM resources and cleanup.

## OPERATIONS

`probe(path)` opens/closes KVM API version 12 (`NULL` selects `/dev/kvm`) and
returns CT success, EIO or ENOTSUP. `backend` constructs an AMD64/ARM64 descriptor;
available performs the actual device probe. Platform-specific guest setup remains
the callbacks' responsibility.

Prepare opens device, VM and vCPU, then invokes configure. Start invokes reset.
Poll performs at most the requested number of KVM_RUN exits, handing each to
exit. Native EINTR/EAGAIN returns AGAIN. The configured exit quota stops with 124.
Signals 9/15 mark the cooperative runner stopped with 137/143; pause prevents new
runs. Neither can interrupt a concurrently executing KVM_RUN, since same-runtime
concurrent/reentrant calls are unsupported.

## LIMITS AND OWNERSHIP

CPU/memory/process/open-file resource requests return ENOTSUP; generic guest
enforcement cannot be inferred from host metadata. KVM exit budgets are not guest
instruction or wall-clock budgets. A guest that never exits can block one poll.
Rootfs is passed to configure but is not automatically booted or mounted.

Destroy closes vCPU, VM, then device; failure retains ownership for retry. No
automatic fallback, external emulator, kernel image or host mount is created.
Tests cover missing-device/invalid-config paths; actual guest execution needs a
usable host KVM device and valid guest callbacks.
