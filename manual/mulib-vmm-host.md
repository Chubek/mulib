# mulib-vmm-host

Concrete host services for the VMM's existing Virtio backend contract. Define
`MULIB_IMPL_ENDIAN`, `MULIB_IMPL_VMMDEV`, `MULIB_IMPL_VIRTIO` and
`MULIB_IMPL_VMM_HOST` once. Example: `examples/modules/vmm-host.c`.

## RAM block storage

`mulib_vmm_host_block_init()` borrows a nonempty, 512-byte-multiple byte array and
optionally copies a 20-byte identifier. The default identifier is
`mulib-ram-disk` padded with zeros. A failed initialization leaves the destination
unchanged. The owner must keep media storage private from guest RAM and alive
until all attached devices are retired. Service input/output must not alias media.

`block_config()` returns the little-endian capacity and matching RO/FLUSH feature
mask. Use those when constructing `mulib_virtio_os_device_t`, and install
`mulib_vmm_host_block_service` with the block object as context. The callback
implements READ, WRITE, FLUSH, ID and RESET and checks address/length overflow,
sector alignment and read-only state independently of the Virtio parser.

The medium is **volatile RAM**. FLUSH completes immediately because writes have
already reached this memory-backed medium; it does not promise disk persistence.
RESET retains media and identity. The service never allocates or frees memory.

## System entropy

`mulib_vmm_host_entropy_service` implements RNG RESET/ENTROPY. Linux uses
`getrandom(..., GRND_NONBLOCK)`, handles EINTR, returns partial successful reads,
and reports AGAIN when the system entropy interface is not ready. It supplies no
deterministic fallback. Other platforms and unavailable system calls return
ENOTSUP for ENTROPY. Context is unused. Invalid device IDs/operations are rejected.

The owner selects advertised services explicitly; this module does not silently
install devices. Other Virtio providers (rendering, sockets, sound, filesystems,
crypto, IOMMU, balloon/memory provisioning) still follow their documented service
contracts. This module does not implement them.

Tests in `tests/test_vmm.c` cover read-only handling, address limits, real system
entropy invocation and end-to-end RAM block I/O. `tests/test_vmm_kvm.c` executes a
native guest that uses the RAM block provider through the VMM call-frame boundary.
