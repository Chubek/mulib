# mulib-vmmdev

Portable, allocation-free MVMD message and guest-memory primitives. The normative
wire and execution contract is [`MULIB_VMMPROTO.md`](../MULIB_VMMPROTO.md).

## Integration

Define `MULIB_IMPL_ENDIAN` and `MULIB_IMPL_VMMDEV` in one translation unit before
including `mulib-vmmdev.h`; other clients include declarations normally.
Example: [`examples/modules/vmmdev.c`](../examples/modules/vmmdev.c).

`mulib_vmmdev_message_t` is a decoded view. Its payload borrows input packet
storage; it is not a struct to cast onto guest memory. Encode/decode enforce the
40-byte header, version 1, exact payload length and 4096-byte packet limit.
On decode failure the output object is unchanged. On short encode capacity the
destination is unchanged and capacity reports the required size. Use separate
payload and destination storage.

## Memory and ownership

Call `mulib_vmmdev_memory_validate()` before publishing a region inventory.
Validation rejects physical and host aliases, empty mappings, overflow and bad
permissions. Keep the validated inventory immutable. `map()` is a lookup against
that trusted inventory, not a replacement for validating it. A span cannot cross
region boundaries, even when regions are adjacent. The caller owns every byte.

`chain_decode()` constructs a borrowed chain from 1..64 serialized spans. All
readable spans precede writable spans; both streams are bounded by `limit` and
all spans are nonempty and disjoint. `chain_read()`/`chain_write()` accept only
successfully decoded, unmodified chains. Offset/size failures occur before
copying. Source/destination buffers must not overlap chain storage.

The VM execution lock must prevent concurrent modification of mappings, packet
metadata and submitted input. The higher-level backend snapshots readable input;
these low-level helpers themselves do not copy or lock guest storage.

Failures are `MULIB_VMMDEV_*` result codes. No allocation can fail because no
function allocates. Size/address overflow and insufficient caller storage are
explicitly rejected. Tests: `tests/test_vmmdev.c`.
