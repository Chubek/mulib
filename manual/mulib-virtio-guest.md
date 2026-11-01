# mulib-virtio-guest — VMM device frontend

The frontend implements discovery, reset, feature/status negotiation,
configuration snapshots and bounded queue submission over
[`MULIB_VMMPROTO.md`](../MULIB_VMMPROTO.md). It is portable C11 and contains no
architecture-specific hypercall instruction.

## Integration

Define `MULIB_IMPL_ENDIAN`, `MULIB_IMPL_VMMDEV`, `MULIB_IMPL_VIRTIO` and
`MULIB_IMPL_VIRTIO_GUEST` once before including `mulib-virtio-guest.h`. For compiled
integration link `mulib_virtio_guest` and include `virtio-guest.h` from
`library/virtio-guest`. The [example](../examples/modules/virtio-guest.c) also
defines `MULIB_IMPL_VIRTIO_OS` to run an in-process backend.

## Ownership and initialization

The application supplies an exchange function, its context, an instance ID,
an optional-feature mask, a guest physical scratch address and its local pointer
and capacity. This address must refer to the same memory that the backend maps;
it is not a host pointer cast to an integer. At least 512 scratch bytes are required.
The client borrows this storage and allocates nothing. Keep it alive and serialize
all accesses while the client is attached. Different active clients need separate
DMA workspaces unless the caller serializes their use.

Envelope encoding/decoding uses bounded automatic arrays. Allow roughly 16 KiB
of call-stack headroom for nested attach/status/exchange paths in addition to
caller objects; this implementation is not tuned for small kernel stacks.

`attach()` probes, validates the known profile, resets the remote device,
negotiates supported requested features plus mandatory features and ends at
FEATURES_OK. Call config helpers for device-specific setup, then `start()`.
Optional features not offered are ignored; asking for a feature not implemented
by this profile is ENOTSUP. Failure zeroes the output client. Never attach over
an active client without first resetting it.

`reset()` advances the epoch and invalidates submission. Reattach before using
the device again. An old client cannot submit after another owner resets the
device. Do not automatically replay a failed write: only AGAIN guarantees that
the provider did not consume it. Tokens detect mismatched replies but do not
deduplicate requests.

## Queue APIs

`submit()` sends caller-managed guest DMA spans. These may point anywhere in
the mapped permitted guest RAM, not only in the frontend workspace. The caller
owns layout and native device-response parsing. The function validates ordering,
length sums, queue range and completion length. The backend additionally checks
mapping permissions, overlap, framing and device semantics.

`transact()` is a copying convenience API. It concatenates readable input then
writable response capacity inside the workspace, submits up to two spans, and
copies the used response bytes back. Thus `input_size + response_capacity` must
fit scratch, and each stream must fit the device limit. Ordinary input/output
buffers must not overlap scratch. Output/capacity remain unchanged on error.
Use this API for the complete wire request/response of SCSI, GPU, input, RPMsg,
vsock, crypto, IOMMU, memory, sound, FUSE, 9P, RPMB, SCMI and balloon profiles.
The [backend manual](mulib-virtio-os.md) describes the supported layouts and
required host providers. Device-specific status remains in the output.

Convenience operations decode native statuses and return `MULIB_VMMDEV_*`:

* `entropy()` accepts short entropy reads and returns actual length.
* `block()` supports type 0 read, 1 write, 4 flush and 8 get-ID. Read/write sizes
  are multiples of 512, sector is in 512-byte units, get-ID is exactly 20 bytes,
  and flush has zero size/sector. Errors leave the caller's read buffer unchanged.
* `send()`/`receive()` support network, console, static RPMsg and input. Network
  callers provide raw Ethernet frame bytes; the helper adds/removes the modern
  12-byte Virtio header. RPMsg callers provide their full 16-byte framing header.
* `gpio()` gets/sets direction/value (types 2..5), validating returned values.
* `i2c()` handles one segment and its status. Address is wire-encoded, flags bit 1
  means read, bit 0 chains failure to the next segment. Zero-length segments work.
  The caller preserves group ordering; a failed group must still be drained.
* `pmem_flush()` sends FLUSH and checks its status.

Config read/write functions enforce bounds. Reads record the response's atomic
snapshot generation. Writes are restricted by the backend to the profile's
driver-writable fields; no arbitrary configuration mutation is implied.

## Limits and verification

The client supports only profiles marked P in the
[support matrix](../MULIB_VMMPROTO.md#6-device-support-matrix). The generic queue
API is a transport frontend, not a filesystem, socket, audio or renderer application
API. There is no PCI/MMIO transport, ring memory, async completion API or kernel
integration. Resource/feature limits and provider requirements are explicit.

Run `python3 tools/check_virtio.py --sanitizers`. Tests cover successful
frontend/backend exchanges, malformed responses, status/epoch failures and all
implemented profiles. Standalone C and C++ declarations are checked separately.
