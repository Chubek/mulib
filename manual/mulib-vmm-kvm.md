# mulib-vmm-kvm

Linux x86 type-2 execution adapter, composed with `mulib-kvm` and `mulib-vmm`.
It runs real guest instructions using `/dev/kvm`, one vCPU and caller-owned RAM.

## Integration

Define `MULIB_IMPL_ENDIAN`, `MULIB_IMPL_VMMDEV`, `MULIB_IMPL_VIRTIO`,
`MULIB_IMPL_VIRTIO_OS`, `MULIB_IMPL_VMM_MEMORY`, `MULIB_IMPL_VMM`,
`MULIB_IMPL_KVM` and `MULIB_IMPL_VMM_KVM` in one translation unit.
See the runnable `examples/modules/vmm-kvm.c` and the full native block-I/O guest
in `tests/test_vmm_kvm.c`.

Initialize `mulib_vmm_kvm_t` with `MULIB_VMM_KVM_INIT`. Put
`mulib_vmm_kvm_executor(&kvm)` into the VM options, initialize the VM, publish its
MVID inventory with `inventory()`/`load()`, and load flat guest code.
Then `mulib_vmm_kvm_open(&kvm, vm.ram, entry, stack, inventory_address)` creates
the VM and vCPU and configures real-mode entry registers.

RAM addresses, sizes and host pointers must be native-page aligned. No region
may reach 0xfffbd000; the upper range is reserved for KVM's TSS. All CPU regions
are readable; read-only slots require KVM_CAP_READONLY_MEM. Entry and stack are
16-bit offsets below 64KiB. The byte at entry, two bytes below stack and the
16-byte inventory header must be mapped with appropriate permissions. The code
and complete inventory must have been loaded by the host. The module owns only
native descriptors and the `kvm_run` mapping, never the guest RAM allocation.

## Native guest ABI

* CS/DS/ES/FS/GS/SS selector and base are zero; segment limits are 0xffff.
* CR0 starts at 0x10; paging/long mode are off; RFLAGS=2 (interrupts disabled).
* IP=`entry`, SP=`stack`, ESI=`inventory_address`; remaining general registers zero.
* The guest sets EAX to a 32-bit physical call-frame address and DX=0x5658, then
  executes **OUT DX,EAX**. In 16-bit code the instruction is bytes `66 EF`.
* The runtime dispatches the frame and writes result/length to frame offsets
  24/28. Registers are not used to return device status. On resumption execution
  continues after OUT. HLT returns a HALT exit to the host.

See `MULIB_VMMPROTO.md` for frame bytes and packet ownership. Frame contents carry
64-bit GPAs even though this native port binding passes a 32-bit frame address.
Port accesses of other sizes, string/repeated I/O, other ports, MMIO and unknown
exits fail explicitly. No emulated legacy devices, BIOS or interrupt controller
are silently installed. This initial engine is intended for flat bare-metal
guests using the MuLIB binding, not an unmodified general-purpose OS boot path.

KVM port exits finish their architectural completion on a subsequent KVM_RUN.
The adapter tracks this and drains pending I/O with `immediate_exit` before a
register reset, so a serviced request is never dispatched again by reset.
KVM_CAP_IMMEDIATE_EXIT is therefore required. Reset restores entry registers and
segment/control configuration; it does not clear RAM or claim a complete machine
power cycle/MSR/device reset. The runtime separately resets its devices.

## Errors, execution and cleanup

`open()` reports ENOTSUP for unavailable KVM/unsupported platforms or required
capabilities, EACCESS for access denial, and EIO for native setup failures.
`system_error` retains native errno. Invalid configuration is EINVAL/EBOUNDS.
Failures close any acquired resources; close can be retried if native unmapping
fails. Close is idempotent after success.

`step()` invokes KVM_RUN once; EINTR/EAGAIN becomes YIELD. Unexpected exits retain
the native exit reason for diagnostics. A guest without exits can run indefinitely;
the embedding application must arrange signal-driven interruption if needed.
Do not install a signal handler that calls runtime operations or frees RAM.
The runtime's stop operation takes effect only between exits.

Close the VMM runtime first (retiring devices), then close the KVM adapter, then
release RAM/provider storage. Never run the borrowed executor concurrently with
runtime calls. A second vCPU is not supported by this adapter.

The native regression executes negotiation and block read/write through nine
port exits, halts, and verifies reset with pending I/O. It prints an explicit skip
when KVM is unavailable; set `MULIB_REQUIRE_KVM=1` to make that a failure. CTest
marks the skip separately. Native tests actually passed on the development host.
