# mulib-virtio

Shared Virtio payload contract for MuLIB's future type-2 VMM. Include either
`virtio.h` or the canonical `mulib-virtio.h`. The former is a public entry point,
not a second implementation.

Define `MULIB_IMPL_ENDIAN`, `MULIB_IMPL_VMMDEV`, and `MULIB_IMPL_VIRTIO` in exactly
one translation unit. No implementation switch enables dependencies implicitly.
Example: [`examples/modules/virtio.c`](../examples/modules/virtio.c).

## Registry

`mulib_virtio_profile(id)` returns borrowed immutable metadata or NULL for holes
and unknown IDs. `profile_count()` and `profile_at()` enumerate all 48 requested
IDs in ascending order. Names and assignments are identifiers, not an assertion
of support. Inspect `support`: only `MULIB_VIRTIO_PROFILE` may be instantiated.
`SPEC_REQUIRED` entries return ENOTSUP at backend initialization; `PENDING` is
reserved for a pinned but unfinished implementation.

The [full support matrix](../MULIB_VMMPROTO.md#6-device-support-matrix) describes
21 working, service-assisted profiles and the 27 unimplemented IDs. Optional
feature masks are deliberately narrow. Metadata excludes the mandatory common
VERSION_1 bit; `required_features` contains mandatory device-specific bits.
9P has a variable mount-tag configuration and its metadata minimum is zero;
the backend requires an exact nonempty tag.

## Wire helpers and services

`blk_header()` encodes type/reserved/sector, with 512-byte sector units.
`input_event()` encodes type/code/value. These two functions require valid output
arrays of 16 and 8 bytes respectively. `gpio_request()` and `i2c_header()` validate
their operations and output pointers before writing 8-byte headers. I2C addresses
are wire-encoded (e.g. seven-bit address 0x50 is passed as 0xa0), not raw addresses.

All multi-byte fields use explicit byte codecs, not packed structs. The shared
service request type gives host providers bounded byte spans, operation metadata
and, for GPU/crypto, a borrowed memory inventory/key. See
[`mulib-virtio-os`](mulib-virtio-os.md) for every provider obligation and
[`mulib-virtio-guest`](mulib-virtio-guest.md) for frontend ownership.

The implementation is allocation-free, has no process globals with mutable state,
and supplies no implicit host I/O. Errors use the shared `MULIB_VMMDEV_*` namespace.
`tests/test_virtio.c` checks the registry, codecs, all implemented device profiles
and negative transport paths. It does not certify full Virtio conformance.
