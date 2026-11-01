# mulib-vmm

The type-2 VMM runtime owns lifecycle and serialized access to the previously
implemented MVMD device layer. It adds copied boot/device inventories, explicit
per-device DMA allowlists, a guest-physical call boundary and an execution-engine
interface. The native engine is [`mulib-vmm-kvm`](mulib-vmm-kvm.md).

## Integration

Define these switches in exactly one C11 translation unit:

```
MULIB_IMPL_ENDIAN
MULIB_IMPL_VMMDEV
MULIB_IMPL_VIRTIO
MULIB_IMPL_VIRTIO_OS
MULIB_IMPL_VMM_MEMORY
MULIB_IMPL_VMM
```

The standalone `examples/modules/vmm.c` also defines `MULIB_IMPL_VMM_HOST` and
`MULIB_IMPL_VIRTIO_GUEST` to demonstrate a RAM disk accessed through the runtime.
Each module can also be implemented in a separate translation unit; dependencies
are never enabled implicitly.

## Construction and ownership

Initialize `mulib_vmm_t` with `MULIB_VMM_INIT` (zero). Construct initialized,
status-zero Virtio backend devices and fill `mulib_vmm_options_t`:

* `ram` describes CPU-visible guest RAM, with READ permission on every region.
* Each `devices[]` entry pairs a backend with an explicit DMA view. Empty means
  **no DMA**, not full RAM. A view must use permission-reduced subregions of RAM.
* `executor` optionally provides `step` and `reset`, both or neither.
* `lock`/`unlock` optionally serialize the entire VM, both or neither.

`init()` copies inventory metadata, validates IDs and mappings, rebases its
internal pointers, and enters READY. It leaves the destination unchanged on
failure. Capacity is 32 CPU regions, 32 devices and 32 DMA regions per device.
The fixed object also contains two 4096-byte packet buffers; inspect `sizeof`
when budgeting storage. Init uses one temporary VM object on the stack.

RAM, backend devices, backend scratch, provider contexts, executor and lock
contexts remain borrowed. Keep them alive through successful `close()`, and
through native-engine close for RAM. Never copy an initialized VM or mutate its
internal fields. Devices cannot be shared between VMs. Host objects and backend
scratch must not overlap guest RAM. Provider-specific private storage, such as
RAM disk media, is the provider owner's responsibility.

All runtime operations acquire the optional lock. Executor callbacks run while
holding it, so guest memory cannot race another runtime operation. Native vCPUs
must never be run concurrently outside this interface. With no lock callbacks,
the caller promises single-threaded, non-reentrant use. A `busy` check rejects
recursive dispatch; it is not a thread lock. Providers must not call back into
the runtime while holding a non-recursive lock. Lock callbacks return shared
`MULIB_VMMDEV_*` codes; failed acquisition has no VM side effects.

## Guest call boundary

See [the protocol runtime binding](../MULIB_VMMPROTO.md#9-vmm-runtime-and-native-binding).

`call()` takes request GPA/size and response GPA/capacity. Both packets must fit
single CPU RAM regions. It validates all response storage **before dispatch**,
rejects overlaps, snapshots the request into private memory, selects only the
target device's DMA view, and copies the completed response back. Its return is
the MVMD transport result; the packet itself may carry a device-protocol error.
Capacity is bytes written, zero on most errors, or required size on ENOSPC.

`trap()` takes a 32-byte read/write call frame GPA. The frame contains request
GPA:u64 at 0, response GPA:u64 at 8, request length:u32 at 16, response capacity:u32
at 20, transport result:u32 at 24 and actual/required length:u32 at 28. It
snapshots arguments and writes result/length back after dispatch. The frame,
request, response capacity and all submitted DMA spans must be disjoint. If the
frame itself is valid, a failed operation is reported **in the frame** and
`trap()` returns OK. An invalid frame returns a transport error directly.

`exchange()` is the equivalent host-buffer adapter, compatible with the existing
Virtio frontend. Its packet buffers must be disjoint, private host storage,
outside the VM object. All these interfaces dispatch a request once per call;
they do not retry callbacks or deduplicate tokens. Repeating a call repeats its
effects. `dispatches` counts backend dispatch attempts, including protocol errors
and short response capacity; exhaustion fails rather than wraps.

The VM checks packet transport, not arbitrary provider behavior. Services remain
trusted host code with explicit capacities and side-effect requirements.

## Lifecycle and execution

* `start()`: READY to RUNNING; requires an executor.
* `step()`: execute one engine exit under the lock. CALL dispatches exactly one
  frame; HALT enters HALTED; YIELD remains RUNNING. Invalid exits, engine failures
  and inaccessible call frames enter FAULTED. The exit records native reason and
  the runtime result. A device error stored in a valid call frame is not a CPU fault.
* `stop()`: RUNNING to READY between exits; retains CPU/device state. READY is
  also accepted. It does not asynchronously interrupt a running engine.
* `reset()`: reset all device epochs, then reset execution registers, then READY.
  RAM/media survive. A failure leaves FAULTED; earlier devices may already have
  reset. Retry is allowed while no device has been retired.
* `close()`: retire devices, erase private packet buffers and enter CLOSED.
  It uses host retirement even when epochs are exhausted. Successful providers
  are not called again on a retry after a later provider fails. Partial retirement
  leaves FAULTED; retry close, not execution/reset. Repeated successful close is
  harmless. The executor is borrowed and must subsequently be closed by its owner.

HALTED/FAULTED engines require reset before start. READY supports host exchanges
and loading; RUNNING supports device access between exits. `load()` performs
validated, transactional cross-region image copies only while READY.
`request_memory()` serializes a host balloon/memory target change.

`inventory()` serializes MVID version 1 into a host buffer: 16-byte header with
magic `MVID`, version:u16=1, reserved:u16=0, count:u32, total size:u32, followed
by 8-byte `{instance:u32, device_type:u32}` records. Use `load()` to publish it to
guest RAM before start. The inventory is boot configuration, not a new MVMD opcode.

## Verification and limits

`tests/test_vmm.c` covers isolation, malformed frames, response preflight,
read-only memory, lifecycle/lock failures, reset retries, epoch exhaustion and
real RAM block services. `tests/test_vmm_kvm.c` tests the same boundary from a
native guest. `tools/check_virtio.py --sanitizers` verifies the entire stack.

This is a fixed-inventory single-executor runtime. It does not implement hotplug,
migration, a guest kernel, firmware, an ELF boot policy or a general-purpose
multi-vCPU scheduler. Other host services retain the explicit support matrix.
