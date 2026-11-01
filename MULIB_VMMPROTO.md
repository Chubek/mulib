# MuLIB VMM device protocol — version 1

This is the implemented device boundary for the **type-2 MuLIB VMM**.
The VMM runs as a host process; `virtio-os` implements its backend and
`virtio-guest` implements the frontend. This document specifies device messages,
memory access, lifecycle and host-service obligations. Section 9 specifies the
runtime and Linux/x86 KVM execution binding. A guest OS and VM launcher remain
the embedding application's responsibility.

The authoritative interfaces are:

* [`include/mulib-vmmdev.h`](include/mulib-vmmdev.h): message codecs and DMA validation.
* [`include/virtio.h`](include/virtio.h): public Virtio contract entry point.
* [`include/mulib-virtio.h`](include/mulib-virtio.h): IDs, support registry and services.
* [`library/virtio-os`](library/virtio-os): backend library.
* [`library/virtio-guest`](library/virtio-guest): frontend library.
* [`include/mulib-vmm.h`](include/mulib-vmm.h): runtime, inventory and guest-call dispatch.
* [`include/mulib-vmm-memory.h`](include/mulib-vmm-memory.h): RAM access and DMA policy.
* [`include/mulib-vmm-host.h`](include/mulib-vmm-host.h): RAM block and system entropy providers.
* [`include/mulib-vmm-kvm.h`](include/mulib-vmm-kvm.h): native Linux/x86 executor.

The transport is MuLIB-specific. It carries little-endian Virtio device payloads
in bounded logical queues. It does **not** expose PCI/MMIO registers or standard
split/packed rings. Existing Linux/QEMU Virtio drivers need a transport adapter;
they cannot use MVMD messages directly. `VERSION_1` selects modern device payload
layouts within this transport, not certification of an OASIS transport.

## 1. Execution and ownership

1. The VMM creates devices with immutable nonzero instance IDs. Multiple instances
   of one device type are allowed. Its VM boot configuration supplies the instance
   inventory and the guest-call mechanism. Section 9 assigns a port binding and
   boot inventory for the initial Linux/x86 executor.
2. Guest calls pass request bytes and response capacity across the VMM boundary.
   A trap adapter must copy the message into private host storage before
   dispatch, validate/copy the response, and marshal guest addresses rather than
   exposing host pointers. `mulib_vmmdev_exchange_fn` is the in-process adapter ABI.
3. The VMM owns all device objects, service contexts, region inventories and scratch
   buffers. The libraries allocate nothing and never free caller storage. Keep
   all borrowed storage alive and inventories immutable until the bus is retired.
4. One VM execution lock serializes device calls, host configuration changes,
   memory-map changes and guest writes to submitted memory. The reference endpoint
   is synchronous and non-reentrant; its `busy` member is a recursion check, not
   an atomic lock. Multi-vCPU implementations must supply synchronization.
5. Every call finishes before the next call is dispatched. A backend service may
   return `AGAIN` only before consuming input or changing external state. No
   pointers, in-flight chains or completion callbacks survive the call.
6. Logical queue indices and ordering are device-specific. Each accepted `SUBMIT`
   is one buffer chain. There is no ring capacity, queue-setup operation or
   interrupt suppression negotiation in version 1. Receive queues are polled;
   `AGAIN` means retry when the host scheduler indicates work may be available.
7. The transport adapter must provide **at-most-once dispatch**. Tokens correlate
   responses; they are not a replay cache. Replaying a successful write repeats
   its effects. An ambiguous transport failure cannot be automatically retried.

The C ABI uses C11, 32/64-bit `size_t`, eight-bit bytes and C linkage for C++
declarations. Native C structures are never wire structures. Scalar codecs work
on unaligned byte buffers and do not depend on host endian order.

## 2. MVMD envelope

All integers are unsigned and little-endian. The header is exactly **40 bytes**.

| Offset | Width | Field |
| ---: | ---: | --- |
| 0 | 4 | ASCII `MVMD` |
| 4 | 2 | Protocol version, exactly 1 |
| 6 | 2 | Opcode; responses set bit 15 |
| 8 | 4 | Device instance ID, distinct from Virtio device type ID |
| 12 | 4 | Device epoch |
| 16 | 8 | Caller token, echoed unchanged |
| 24 | 8 | Request argument; response result code |
| 32 | 4 | Request count; response auxiliary value |
| 36 | 4 | Payload length, excluding the header |
| 40 | variable | Payload, exactly the declared length |

Maximum total packet size is **4096 bytes**. Trailing bytes, truncation, wrong
magic and unsupported versions fail decoding. No pointer, `size_t`, native enum,
padding or platform `errno` is serialized. RPMB payload fields deliberately keep
the big-endian order required by RPMB; their MVMD envelope is little-endian.

The C exchange function returns a transport/codec result. On transport success,
the caller must also inspect the result in the response argument. Malformed
envelopes have no response. Valid envelopes with invalid operations normally
receive a response carrying a device-protocol error. Errors have zero payload
length and zero auxiliary value. Successful responses echo opcode, instance and
token and contain the current epoch.

`encode()`/`exchange()` return `ENOSPC` and the required response size when the
destination is short. The backend checks response capacity **before side effects**.
Request, response and codec payload storage must not overlap unless explicitly
documented otherwise.

### Results

| Value | Name | Meaning |
| ---: | --- | --- |
| 0 | OK | Operation completed |
| 1 | EINVAL | Malformed argument, framing or descriptor |
| 2 | EVERSION | Unsupported MVMD version |
| 3 | ENODEV | Unknown instance/type |
| 4 | ESTALE | Device epoch mismatch |
| 5 | ESTATE | Invalid lifecycle transition |
| 6 | ENOTSUP | Unsupported operation, feature or device profile |
| 7 | EBOUNDS | Address, queue, configuration or transfer limit exceeded |
| 8 | EACCESS | Wrong DMA/configuration permissions |
| 9 | EIO | Host service or response validation failure |
| 10 | AGAIN | Not consumed; retry later |
| 11 | ENOSPC | Insufficient caller/provider capacity |

Device-specific status bytes/words are separate from these results. For example,
block I/O failure completes the queue request with a block status of 1, and a
busy virtio-mem operation returns its protocol's BUSY response. A provider failure
can have external side effects; only `AGAIN` guarantees non-consumption.

## 3. Operations

Unused arguments, counts and payloads must be zero/empty. `PROBE` uses epoch zero;
all other operations require the current device epoch.

| Opcode | Operation | Argument | Count | Payload | Successful response |
| ---: | --- | --- | --- | --- | --- |
| 1 | PROBE | 0 | 0 | empty | 32-byte discovery record |
| 2 | RESET | 0 | 0 | empty | new epoch, empty payload |
| 3 | STATUS | complete new status byte | 0 | empty | empty |
| 4 | FEATURES | accepted 64-bit feature mask | 0 | empty | empty |
| 5 | CONFIG_READ | byte offset | byte count | empty | config bytes; auxiliary=config generation |
| 6 | CONFIG_WRITE | byte offset | 0 | config bytes | empty |
| 7 | SUBMIT | logical queue index | span count | serialized spans | auxiliary=written byte count |

The `PROBE` payload has these fields:

| Offset | Width | Meaning |
| ---: | ---: | --- |
| 0 | 4 | Virtio device type ID |
| 4 | 4 | Status byte, zero extended |
| 8 | 8 | Offered feature bits |
| 16 | 4 | Number of logical queues |
| 20 | 4 | Configuration size |
| 24 | 4 | Configuration generation |
| 28 | 4 | Maximum bytes in each readable/writable stream |

Configuration reads are atomic snapshots under the VM lock. Generation increases
when configuration changes and may wrap modulo 2^32. Config changes are polled;
there are no asynchronous configuration interrupts in this version. Maximum
configuration size is 256 bytes; per-device limits are in the support matrix.

### Lifecycle

New backend devices start at epoch 1, status 0. Frontends use:

```
PROBE → RESET → STATUS(1) → STATUS(3) → FEATURES(mask)
      → STATUS(11) → device configuration → STATUS(15) → SUBMIT…
```

The status bits retain Virtio meanings: ACKNOWLEDGE=1, DRIVER=2, DRIVER_OK=4,
FEATURES_OK=8, DEVICE_NEEDS_RESET=64 and FAILED=128. Normal transitions are exactly
`0 → 1 → 3 → 11 → 15`; repeating a status is allowed. A driver may set FAILED
while preserving existing bits. Failed/reset-needed devices require RESET.

Features are a subset of the offered mask, include bit 32 (`VERSION_1`) and
include any profile-mandatory bits. FEATURES is allowed only at status 3.
It is frozen after FEATURES_OK. Unsupported transport features are never offered.
No queue request is consumed before status 15. Driver-written configuration
requires FEATURES_OK and a profile-defined writable field.

RESET calls the provider synchronously, then clears negotiated features, status
and transient state and increments the epoch. Stale requests cannot cross reset.
Epoch UINT32_MAX cannot reset: retire the instance rather than reuse an epoch.
Providers must release/reset external resources before returning success. If a
provider fails reset, the device enters NEEDS_RESET without advancing the epoch.
Persistent block/PMEM/RPMB contents survive. Virtio-mem plugged blocks survive a
device reset and are reconciled with UNPLUG_ALL; system reset is a VMM policy.

The frontend `attach()` ends at status 11; `start()` advances to 15. A failed
partially completed attach attempts to set FAILED. `reset()` ends frontend use;
reattach to negotiate again. Do not attach two owners to the same instance.

## 4. Guest memory and buffer chains

The VMM installs an immutable list of regions `{guest address, size, host data,
device permissions}`. Region permissions are READ=1 and WRITE=2. Regions must be
nonempty, non-overflowing and disjoint in both guest and host address spaces.
Only actual guest RAM belongs in this inventory; host device metadata, keys,
scratch, code and protocol packets must never be mapped as guest RAM.

Each SUBMIT span occupies **16 bytes**:

| Offset | Width | Meaning |
| ---: | ---: | --- |
| 0 | 8 | Guest physical address |
| 8 | 4 | Nonzero length |
| 12 | 4 | 0=device-readable, 1=device-writable; all other values invalid |

There are 1..64 spans. Readable spans precede writable spans. Every span fits
entirely inside one region with appropriate permissions. No spans in one chain
may overlap. There are no NEXT, INDIRECT or packed-ring flags: the table is
already a bounded, flattened chain. Each concatenated stream must fit the
device's transfer limit, configured between 512 bytes and 1 MiB.

The backend validates the **entire chain first**, snapshots the readable stream
into private scratch, calls the device handler, and copies only completed output
to writable spans. Arbitrary descriptor boundaries within each stream are allowed.
Malformed later descriptors cannot cause an earlier write or service call.
The completion count is the number of bytes copied to the writable stream.
When a response has a trailing status, failure paths zero preceding output and
return the complete response length; callers must still inspect that status.

GPU backing entries are a separate persistent resource association. They are
validated against the same guest RAM inventory on attachment; the renderer may
copy GPA metadata, but must resolve/read it only during a serialized GPU call.
The inventory must remain stable while those associations exist.

## 5. Host services

All implemented profiles require a `mulib_virtio_service_fn`. The callback
receives decoded operation metadata and bounded input/output buffers. It returns
a protocol result and an exact output count. It must respect output capacity;
the count check cannot repair a callback that has already written out of bounds.
Returned counts and protocol responses are validated before copying into guest RAM.

The host provides real disk/file/network/entropy/render/audio/crypto facilities.
The reference libraries do not open host devices implicitly. A renderer owns its
surfaces; a socket provider owns connection and credit state; an IOMMU provider
owns domains, translation and invalidation; an RPMB provider owns authenticated
storage/key/counter semantics. These are required services, not optional success
stubs. Their precise contracts are in [the backend manual](manual/mulib-virtio-os.md).

The VMM must implement isolation and device-to-region policy around those
services. In particular, validating the IOMMU request wire format does not itself
install mappings in the native vCPU engine. Likewise, balloon PFNs and virtio-mem
plug/unplug callbacks must actually perform the requested memory ownership changes.

## 6. Device support matrix

**P** = implemented bounded frontend/backend profile, with the host services and
feature limits below. **U** = registered ID, unimplemented; initialization returns
`ENOTSUP`. No U entry has a dummy successful backend. All 48 requested IDs are
represented; IDs 0, 14 and 15 remain invalid/unassigned here.

| ID | Device | State | Implemented profile or outstanding specification |
| ---: | --- | :---: | --- |
| 1 | virtio-net | P | One RX/TX pair, modern 12-byte header, optional MAC; no offloads/MQ/control queue |
| 2 | virtio-blk | P | Read/write/flush/get-ID, 512-byte sectors; optional RO and FLUSH |
| 3 | virtio-console | P | Single console RX/TX, no multiport or emergency-write feature |
| 4 | virtio-rng | P | Entropy reads from required provider; short reads supported |
| 5 | traditional balloon | P | Inflate/deflate PFNs, target/actual config; MUST_TELL_HOST required |
| 6 | ioMemory | U | Historical allocation; select an authoritative wire specification |
| 7 | RPMsg | P | Static endpoints, 16-byte Linux RPMsg framing; no name-service feature |
| 8 | virtio-scsi | P | One command queue, TMF control, configurable sense/CDB sizes; no hotplug/PI/bidirectional I/O |
| 9 | virtio-9p | P | Mount tag and framed 9P request/reply transport; provider supplies filesystem semantics |
| 10 | mac80211 WLAN | U | Pin implementation/version and configuration/queue ABI |
| 11 | remoteproc serial | U | Pin remoteproc firmware/host serial-link ABI |
| 12 | CAIF | U | Pin CAIF framing and device ABI |
| 13 | memory balloon | U | Distinct historical ID; must not alias traditional balloon |
| 16 | virtio-gpu | P | 2D resources, backing, transfer, scanout, flush, cursor and fences; renderer required |
| 17 | RTC / historical clock | U | Choose RTC draft revision; historical clock is not an interchangeable ABI |
| 18 | virtio-input | P | Event/status queues and selector/subselector configuration |
| 19 | virtio-vsock | P | Stream packet/event transport; provider must implement sockets, replies and credit accounting |
| 20 | virtio-crypto | P | AES-CBC session framing, 16/24/32-byte keys, 16 sessions; real cipher provider required |
| 21 | signal distribution | U | Select authoritative protocol and signal model |
| 22 | pstore | U | Pin storage record/configuration ABI |
| 23 | virtio-iommu | P | 4 KiB MAP/UNMAP, ATTACH/DETACH, fault queue; VMM mapping provider required |
| 24 | virtio-mem | P | Plug/unplug/unplug-all/state; up to 2048 blocks; no optional features |
| 25 | virtio-snd | P | Two PCM streams, stereo S16 LE at 48 kHz, lifecycle and message I/O; no jacks/chmaps/shared-memory |
| 26 | virtio-fs | P | FUSE transport, one request queue plus hiprio forget queue; no DAX/notification feature |
| 27 | virtio-pmem | P | Persistent range config and flush completion |
| 28 | RPMB | P | 512-byte big-endian frame transport and combined result-read; authenticated RPMB provider required |
| 29 | hwsim | U | Pin Linux wireless simulation/netlink payload ABI |
| 30 | video encoder | U | Pin standalone video proposal; not interchangeable with ID 50 |
| 31 | video decoder | U | Pin standalone video proposal; not interchangeable with ID 50 |
| 32 | SCMI | P | Synchronous command channel; no P2A/fast channels/shared-memory statistics |
| 33 | Nitro Secure Module | U | Pin NSM CBOR/attestation contract and provider requirements |
| 34 | I²C | P | Read/write/zero-length segments, encoded 7/10-bit addresses and FAIL_NEXT groups |
| 35 | watchdog | U | Pin timeout, lifecycle and expiry semantics |
| 36 | CAN | U | Pin CAN draft revision and supported bus/controller features |
| 37 | DMA-BUF | U | Pin proposal and cross-device buffer ownership ABI; absent from current OASIS ID table |
| 38 | parameter server | U | Pin schema and request/notification ABI |
| 39 | audio policy | U | Pin policy protocol; not virtio-snd |
| 40 | Bluetooth | U | Pin HCI/transport proposal and device features |
| 41 | GPIO | P | Direction/value operations; no line names or IRQ feature |
| 42 | RDMA | U | Pin verbs, memory registration and queue ABI |
| 43 | camera | U | Pin stream/control/buffer proposal |
| 44 | ISM | U | Pin shared-memory transport and lifecycle ABI |
| 45 | SPI | U | Pin SPI draft revision and transfer capabilities |
| 46 | TEE | U | Pin trusted-service, session and shared-memory ABI |
| 47 | CPU balloon | U | Pin CPU topology, ownership and hotplug proposal |
| 48 | media | U | Pin media/V4L2 proposal and advertised command set |
| 49 | USB | U | Pin controller/topology/transfer proposal |
| 50 | combined video | U | Pin combined codec proposal and buffer/resource model |

This is **21 implemented profiles and 27 unimplemented IDs**, not an assertion
that every listed device or optional feature has been implemented. Several U
entries have published drafts or existing implementations; “pin” means the exact
ABI still needs to be selected for this project, not that documentation does not
exist. Adding one requires a real parser/state machine, both-side tests and a
matrix update; assigning an ID alone must never change support to P.

## 7. References and compatibility policy

* [OASIS Virtio 1.2](https://docs.oasis-open.org/virtio/virtio/v1.2/virtio-v1.2.html):
  baseline modern payloads for the standardized P devices. Optional newer revisions
  (notably crypto revision-1/stateless formats) are not negotiated.
* [OASIS source](https://github.com/oasis-tcs/virtio-spec): device chapters were
  consulted for layout/behavior; the ID table at commit
  `e6b9307d84880b4e04a92a6ab403eff33db7fd1f` records newer assignments and explicitly
  notes that assignment does not imply specification.
* [Linux RPMsg](https://docs.kernel.org/staging/rpmsg.html) and Linux
  `drivers/rpmsg/virtio_rpmsg_bus.c`: static RPMsg framing.
* Linux `include/uapi/linux/virtio_9p.h`, `virtio_ids.h`, and the 9P/FUSE/SCMI
  specifications define the external transports/protocols named above.

Implementations are original MuLIB code; no third-party implementation source
was vendored. Constants and layouts express protocol interoperability facts.
MVMD version 1 is independently versioned from Virtio and those external protocols.
Breaking envelope, ownership or lifecycle changes require a new MVMD version.
Future PCI/MMIO/ring adapters must negotiate their own supported transport features.

## 8. Verification and development

```
python3 tools/check_virtio.py
python3 tools/check_virtio.py --sanitizers
cmake -S . -B build
cmake --build build --target mulib_virtio_os mulib_virtio_guest mulib_test_virtio mulib_test_vmmdev mulib_test_vmm mulib_test_vmm_kvm
ctest --test-dir build -R 'mulib_test_(virtio|vmmdev|vmm|vmm_kvm)$' --output-on-failure
```

Tests exercise message golden bytes, malformed/truncated/overlapping DMA,
permissions, capacity-before-side-effect behavior, feature/status/reset rules,
provider failures, all P profiles and bounded resource/session state. They are
in-process protocol tests. The native KVM regression additionally executes a
guest that negotiates and performs RAM block read/write via the call-frame ABI,
halts and resets with pending port I/O. It reports an explicit skip when KVM is
unavailable; `MULIB_REQUIRE_KVM=1` makes this a failure. External rendering,
cryptographic and hardware providers still require their own integration tests.

## 9. VMM runtime and native binding

### 9.1 Inventory and lifecycle

`mulib-vmm.h` implements a fixed inventory of at most 32 devices, 32 CPU RAM
regions and 32 DMA regions per device. Instance IDs MUST be unique and nonzero.
The runtime copies inventory metadata; RAM, devices, scratch, host services and
the executor remain borrowed. Their addresses and mappings MUST remain stable.
Each device has an explicit DMA view consisting of permission-reduced subregions
of CPU RAM with matching host addresses. An empty view grants no DMA.

The host initializes the runtime in READY, loads RAM and starts the executor to
enter RUNNING. Each step services one executor exit: CALL dispatches one frame,
YIELD retains RUNNING, and HALT enters HALTED. Inaccessible frames and executor
errors enter FAULTED. Stop changes RUNNING to READY between exits. Reset resets
device epochs followed by executor registers and returns to READY; RAM/media
survive. Failed reset leaves FAULTED and may have reset earlier devices.

Close retires devices through `mulib_virtio_os_shutdown()`, including instances
whose epochs are exhausted, erases private packet buffers and enters CLOSED.
Retirement calls the provider RESET operation, erases scratch/session state,
sets epoch zero and prevents further protocol access. It is idempotent after
success; failed retirement may be retried. After partial retirement only close
may complete cleanup. The owner closes the native executor before releasing RAM.

All operations MUST be externally serialized, or use the runtime's paired
lock/unlock callbacks. Executor and service callbacks run under this lock and
MUST NOT re-enter the runtime. No other vCPU may concurrently access its RAM.

Boot discovery uses a little-endian **MVID version 1** blob:

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | ASCII `MVID` |
| 4 | 2 | Version = 1 |
| 6 | 2 | Reserved = 0 |
| 8 | 4 | Device count |
| 12 | 4 | Total bytes = 16 + 8 × count |
| 16 | 8 × count | Records: instance:u32, Virtio device ID:u32 |

The host encodes this with `mulib_vmm_inventory()` and loads it before execution.
The guest MUST validate its version, reserved field, bounds and count before use.
This inventory is immutable boot configuration, not an additional MVMD opcode.

### 9.2 Guest-physical call frame

The version-1 binding uses the following 32-byte little-endian frame:

| Offset | Size | Field |
|---|---|---|
| 0 | 8 | Request GPA |
| 8 | 8 | Response GPA |
| 16 | 4 | Request bytes, 40–4096 |
| 20 | 4 | Response capacity, at most 4096 |
| 24 | 4 | Output transport result (`MULIB_VMMDEV_*`) |
| 28 | 4 | Output bytes written, or required size on ENOSPC |

Frame storage MUST be readable and writable within one CPU RAM region. Request
storage MUST be readable and response capacity MUST be writable, each within a
single region. Frame, request, response capacity and submitted DMA spans MUST
be mutually disjoint as applicable; DMA spans also obey the existing chain rules.
The runtime snapshots frame arguments and the entire request into private memory,
preflights response storage, then dispatches using only the target device's DMA
view. It performs no automatic retries or token deduplication.

For a valid frame, transport failures are stored at offset 24 and `trap()` itself
returns OK. Offset 28 is zero on failure except ENOSPC, which reports required
capacity. An invalid frame fails directly and faults execution. An OK transport
result does not imply device success: the MVMD response retains its own status.
The host-buffer `mulib_vmm_exchange()` adapter exposes equivalent dispatch to the
existing frontend without using a guest frame.

### 9.3 Linux/x86 KVM executor

`mulib-vmm-kvm.h` creates one native vCPU and maps caller-owned page-aligned RAM.
All RAM is readable; read-only mappings require KVM_CAP_READONLY_MEM. RAM MUST
remain below 0xfffbd000, reserving the upper area for KVM's TSS. The initial entry
and stack are 16-bit offsets with zero segment bases/selectors, 0xffff segment
limits, CR0=0x10 and RFLAGS=2. ESI contains the inventory GPA. General registers
other than IP, SP and ESI are zero. This is a flat bare-metal boot contract.

The guest places the frame's **32-bit GPA in EAX**, sets **DX=0x5658**, then executes
**OUT DX,EAX** (`66 EF` in 16-bit mode). One 4-byte non-string output triggers one
call. Results are returned in the frame, not registers. HLT returns control to the
host as HALT. Other I/O sizes/ports, repeated I/O, MMIO and unknown exits fail
explicitly. Existing native Virtio drivers require a transport adapter.

KVM completes a pending port exit on the next KVM_RUN. Reset drains that completion
with `immediate_exit` before restoring entry registers; it MUST NOT redispatch the
already serviced frame. KVM_CAP_IMMEDIATE_EXIT is required. Register reset retains
RAM and is not a complete architectural power cycle. EINTR/EAGAIN yields to the host.
Execution may block until a guest exit or host signal; stop operates between exits.

The supplied host services implement volatile RAM block storage and Linux
`getrandom` entropy. Other providers retain the obligations in the support matrix.
Detailed integration and ownership rules are in the four `manual/mulib-vmm*.md`
runtime manuals and runnable `examples/modules/vmm*.c` examples.
