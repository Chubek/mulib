# mulib-virtio-os — VMM device backend

This header implements the backend of the [MVMD contract](../MULIB_VMMPROTO.md).
It contains lifecycle, feature negotiation, configuration, DMA validation and
21 device-specific bounded profiles. Real host resources are supplied through
an explicit service callback. Unimplemented registry entries return ENOTSUP.

## Integration and lifecycle

Define these switches in one C11 translation unit:

```
MULIB_IMPL_ENDIAN
MULIB_IMPL_VMMDEV
MULIB_IMPL_VIRTIO
MULIB_IMPL_VIRTIO_OS
```

Alternatively link CMake target `mulib_virtio_os` and include `virtio-os.h` from
`library/virtio-os`. It links the separately compiled shared protocol library.
Do not also define these implementations in an application linking that library.
Example: [`examples/modules/virtio-os.c`](../examples/modules/virtio-os.c).

1. Prepare a real `service`, its context, two disjoint private scratch arrays and
   a little-endian configuration array from the table below.
2. Call `mulib_virtio_os_init()`. `features` lists only supported optional and
   mandatory profile features; VERSION_1 is added automatically. The service must
   implement all operations of the chosen profile. There is no auto-detection of
   provider capabilities.
3. Build a fixed array of device pointers and validated guest RAM regions, and
   call `mulib_virtio_os_bus_init()`.
4. Expose `mulib_virtio_os_exchange` with the bus as context to the VMM transport.
5. Call `mulib_virtio_os_shutdown()` before retiring its provider or memory. It
   invokes provider RESET, erases cipher keys, scratch and transient state, and
   sets epoch zero so further protocol operations return ENODEV. This host-only
   operation also works at epoch exhaustion and is idempotent after success.
   Failure retains the epoch and marks NEEDS_RESET; retry before releasing storage.

Objects and buffers are borrowed and caller-owned. Init copies config bytes and
does not modify the destination on failure. Never initialize over a live device,
copy a published device, change its instance, or modify its internal fields.
An active bus's pointer/region inventories cannot move. There is no allocator,
worker thread, hidden device opening or native interrupt injection.
Calls and host configuration updates require an external VM lock.

`transfer_limit` is 512..1048576 bytes for **each** stream; both private scratch
arrays must be at least that long. They cannot alias each other or mapped guest
RAM. Provider output is copied to guest memory only after validation. GPU backing
associations additionally require stable mappings until detached/reset.

## Construction configuration

All unused configuration bytes below are zero. Exact lengths are required.
Fields listed as `u16`, `u32`, `u64` are little-endian at the indicated offsets.

| Device | Bytes | Required fields; allowed features |
| --- | ---: | --- |
| net | 6 | MAC bytes; feature 5 exposes them |
| blk | 8 | capacity:u64 at 0 in sectors, at most UINT64_MAX/512; features 5=RO, 9=FLUSH |
| console | 0 | No features |
| rng | 0 | No features |
| balloon | 8 | target PFNs:u32 at 0, actual:u32 at 4 initially zero; feature 1 required |
| rpmsg | 0 | No features; static endpoints |
| scsi | 36 | u32: queues=1 at 0, seg_max=1..62 at 4, max_sectors>0 at 8, cmd_per_lun>0 at 12, event size=16 at 16, sense=96 at 20, CDB=32 at 24; channel:u16=0 at 28, target:u16<=255 at 30, LUN:u32<=16383 at 32 |
| 9p | 3..256 | tag length:u16 at 0 followed by exactly that many tag bytes; feature 0 required |
| gpu | 16 | events and clear initially zero, scanout count:u32=1..16 at 8, capsets:u32=0 at 12 |
| input | 136 | Initially all zero; selector at 0, subselector at 1, size at 2, data at 8 |
| vsock | 8 | guest CID:u64=3..0xfffffffe; implicit stream type, no optional features |
| crypto | 56 | u32: ready=1 at 0, dataqueues=1 at 4, services=1 at 8, cipher mask=8 at 12, max key=32 at 36; max data bytes:u64 at 48, nonzero and <=limit-88; others zero |
| iommu | 8 | page size mask:u64=4096; feature 2 required |
| mem | 56 | block:u64 at 0 (power of two, >=4096), base:u64 at 16, region:u64 at 24, usable:u64 at 32 equals region, plugged:u64 at 40 initially zero, requested:u64 at 48; aligned sizes, <=2048 blocks |
| snd | 12 | jacks:u32=0 at 0, streams:u32=2 at 4, chmaps:u32=0 at 8 |
| fs | 40 | nonempty NUL-terminated tag in bytes 0..35, request queues:u32=1 at 36 |
| pmem | 16 | base:u64 at 0, nonzero size:u64 at 8, no address overflow |
| rpmb | 3 | capacity in 128KiB units <=128, max write blocks, max read blocks; zero max means no additional device limit |
| scmi | 0 | Synchronous command channel only |
| i2c | 0 | Feature 0 required (zero-length requests and explicit read direction) |
| gpio | 8 | line count:u16>0 at 0; names size=0; no IRQ feature |

`request_memory(device, amount)` changes balloon target PFN count or virtio-mem
requested byte size and increments config generation. It is a host API, not guest
DMA. The guest polls generation/config. Other config is fixed except input
selection, balloon actual, SCSI sense/CDB sizes (1..256), and GPU event clearing.

## Provider contract

`service(context, request, &used)` receives borrowed storage valid for the call.
Initialize `used` to the bytes actually written (the dispatcher initializes it
to zero). Never write beyond `output_capacity`, retain buffer/key/inventory
pointers, or reenter the bus. Copy metadata needed for external state instead.
Return a `MULIB_VMMDEV_*` result. `AGAIN` means no input consumed and no external
side effects; the caller may retry. A successful send/write must consume the
entire input. Zero-capacity operations must return used=0.

Every provider implements RESET, returning zero only after external cleanup.
Persistent media contents survive; sockets, audio streams, renderer resources
and IOMMU domain attachments are reset. Virtio-mem plugging survives device reset.
For balloon reset the provider reconciles/restores outstanding balloon ownership.

### Byte-oriented services

* **Block**: READ/WRITE `address` is a byte offset, already range-checked against
  sector capacity. READ must fill exactly its output capacity; WRITE consumes all
  input. FLUSH returns no bytes. ID returns exactly 20 bytes. The backend enforces
  RO, translates failure into block status, and never performs a write for a
  rejected read-only request.
* **Entropy**: ENTROPY writes 1..capacity real entropy bytes. Return AGAIN if no
  entropy is available. The library supplies no PRNG and never substitutes zeros.
* **Network**: SEND/RECEIVE operate on Ethernet frame bytes with the Virtio header
  removed. RECEIVE returns one complete frame, or AGAIN. Offload metadata is not
  supported. The core creates/validates the modern 12-byte zero header.
* **Console**: SEND consumes bytes; RECEIVE may return a shorter byte sequence.
* **Input**: SEND/RECEIVE operate on 8-byte event multiples. INPUT_CONFIG takes
  select in `address` and subselect in `flags`, and returns 0..128 bytes of selected
  configuration data. Unknown selectors return zero bytes. The provider advertises
  truthful event/code capabilities and implements event synchronization semantics.
* **RPMsg**: SEND/RECEIVE include the complete 16-byte RPMsg header; source and
  destination are u32 at 0/4, reserved=0 at 8, payload length:u16 at 12, flags=0
  at 14. The core validates length and zero reserved/flags fields. The provider
  routes static endpoints; name-service discovery is not advertised.
* **Balloon**: INFLATE/DEFLATE input is a list of u32 PFNs, with 4096-byte page
  units. The provider validates page ownership and completes the whole operation
  before success. The guest reports actual page count through config offset 4.
* **PMEM**: FLUSH must make the configured memory range persistent before success.

### Stateful and external protocol services

* **SCSI**: SCSI_COMMAND receives the complete command stream: LUN[8], tag:u64,
  task attribute/prio/CRN[3], configured CDB bytes, then optional data-out. `address`
  is the tag and `flags` is sense capacity. It returns the full SCSI response:
  sense_len:u32, residual:u32, status_qualifier:u16, status:u8, response:u8, the
  configured sense area, then data-in. Sense length, residual and response ranges
  are checked. SCSI_TMF receives LUN[8], tag in address, subtype in flags and returns
  one response byte. The provider implements target/LUN/CDB semantics; unsupported
  targets/commands produce SCSI responses. No asynchronous events are advertised;
  the event queue returns AGAIN and AN queries/subscriptions report zero events.
* **9P/FUSE/SCMI**: REQUEST takes the complete external message and returns its
  complete response. The core checks framing and tag/unique/header correlation.
  9P uses size:u32, type:u8, tag:u16. FUSE uses its 40-byte request and 16-byte
  response headers; FORGET, BATCH_FORGET and NOTIFY_REPLY have no response. The
  hiprio queue accepts forget operations. SCMI checks a command header, aligned
  words and response header/status; errors have only a status word. Providers
  implement protocol negotiation and operation semantics. SCMI must reject
  asynchronous requests because no P2A feature is offered.
* **Vsock**: SEND/RECEIVE transfer complete 44-byte-header packets on queues 1/0.
  `address` is the guest CID. Header lengths, CID direction, opcode and flags are
  checked, and transmit tail padding is discarded. The provider implements
  connection establishment, stream delivery, shutdown/reset, bounded reply queues
  and `buf_alloc`/`fwd_cnt` credit accounting. It must queue RST for unsupported
  socket types. RECEIVE on queue 2 returns a 4-byte TRANSPORT_RESET event (zero),
  or AGAIN. This is a packet frontend/backend, not a POSIX sockets implementation.
* **IOMMU**: IOMMU receives type 1..4 in `flags`, domain in `address`, and the
  request body (starting with domain) in input. It returns a one-byte Virtio-IOMMU
  status 0..8; the core appends reserved zeros. ATTACH/DETACH bodies are 16 bytes;
  MAP is 32, UNMAP is 24. The core validates sizes, 4KiB inclusive-range alignment,
  overflow and read/write flags. The provider owns endpoint/domain existence,
  overlap rules, map/unmap changes, DMA isolation and invalidation. RECEIVE on
  queue 1 supplies a 24-byte fault record or AGAIN. No bypass/probe feature is
  negotiated. The provider must implement default unattached-endpoint behavior
  required by the selected Virtio-IOMMU specification.
* **Memory provisioning**: PLUG/UNPLUG takes base address and block count in
  `address`/`flags`. The core owns a 2048-bit state map, checks requested capacity,
  duplicate plug/unplug, bounds and state queries, and updates it only after
  success. UNPLUG_ALL calls UNPLUG for the complete usable range, including already
  unplugged blocks. Provider AGAIN is returned as virtio-mem BUSY. The VMM must
  actually make plugged blocks available and unplugged blocks unavailable.
* **Sound**: Stream 0 is playback, stream 1 capture. PCM_CONTROL carries stream
  ID in address and the Virtio command (0x101..0x105) in flags. SET_PARAMS input
  is its 16-byte parameter body; other controls have empty input. The provider
  implements configuration, prepare, release, start and stop. WRITE/READ on
  queues 2/3 carries interleaved stereo S16 LE frames, at 48kHz, with stream ID
  in address. A successful I/O must consume/fill all frames before completion.
  The core enforces state transitions, period bounds and frame alignment. Prepared
  but unstarted I/O returns AGAIN. No intermediate queue is retained by MuLIB;
  schedule retry after START. Latency is reported as zero by this focused profile.
* **Crypto**: The core parses the original fixed 72-byte control/data headers,
  manages up to 16 monotonically numbered sessions and enforces AES-CBC direction,
  key sizes, IV=16 and block-multiple equal input/output lengths. CIPHER receives
  a borrowed `key`/`key_size`, IV[16] followed by plaintext/ciphertext in input,
  flags 0=encrypt/1=decrypt, and an exact destination capacity. A real AES-CBC
  provider must perform the operation without padding. Sessions are erased on
  destruction/reset, and backend request scratch is erased after processing.
  Caller-owned guest request/key buffers must be erased by their owner. The
  module supplies no cryptographic primitive or key persistence. Crypto revision-1,
  stateless mode, hashes, MACs, AEAD, asymmetric operations and chaining are not
  advertised. Tests use a known-vector provider fixture, not a replacement cipher.
* **RPMB**: RPMB receives complete 512-byte big-endian frames. The core validates
  frame counts, request types, configured read/write maxima and optional appended
  RESULT_READ, and validates response type/size. The provider must implement or
  forward to genuine authenticated RPMB: one-time key programming, HMAC-SHA256,
  nonce preservation, write counters, address bounds and persistent results.
  Transport success is not an authentication decision. RPMB status words remain
  in returned frames. Input scratch is erased after processing.
* **GPU**: GPU receives command type in flags, resource ID in address, command
  body after the 24-byte control header in input, and a borrowed validated guest
  memory inventory. The renderer owns allocation, pixel surfaces, backing-entry
  metadata, scanout and cursors. It must finish the operation before success.
  GET_DISPLAY_INFO returns 384 bytes (16 display records); all other operations
  return zero bytes. The core manages 16 resource IDs and dimensions, validates
  backing entries, transfer bounds, rectangles, formats and scanout IDs, and
  constructs error responses/fence echoes. Sizes are limited to 16384x16384;
  the renderer may return ENOSPC before allocating large resources. Each backing
  attachment has at most 64 entries. Cursor images are 64x64. No 3D, EDID, blobs,
  capsets, UUID sharing or context features are advertised. A successful UNREF
  must release associated backing and scanouts. RESET releases all resources.

Provider failure generally maps to the device's native failure status where one
exists; malformed transport/layout errors use MVMD errors. Unsupported commands
are explicitly rejected. No success is manufactured for missing provider work.

## Verification

`tests/test_virtio.c` exercises all P profiles using deterministic providers,
including failure, resource bounds, lifecycle, cipher session invalidation and
DMA errors. `tools/check_virtio.py` independently builds with GCC/Clang and can
enable ASan/UBSan. Real host-service integration needs additional tests in the
future VMM; this module does not claim full device/standards conformance.
