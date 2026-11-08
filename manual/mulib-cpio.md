# mulib-cpio

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_CPIO` once. No dependencies or dynamic allocations.
[Example](../examples/modules/cpio.c).

## Supported formats

Reads and builds SVR4 **newc** (`070701`) and **crc** (`070702`): 110-byte
ASCII headers, thirteen eight-digit hexadecimal fields, NUL-terminated names,
four-byte record/data alignment, and `TRAILER!!!`. Hex input accepts either case;
output is deterministic lowercase. The crc checksum is an unsigned byte sum
modulo 2^32, **not** a cyclic redundancy check. newc writers emit zero checksum;
readers ignore that field as specified for new ASCII CPIO.
Binary and odc formats report UNSUPPORTED; compressed archives are not decoded.

`mulib_cpio_next` takes a caller-owned immutable archive, byte count and cursor
initially zero. It returns 0 for an entry, 1 for a trailer, or a negative error.
EOF without a header returns BOUNDS. Cursor and output only change on a decoded
entry/trailer. It borrows name/data pointers and preserves all metadata, including
inode/device identity, link counts, modes and device-node numbers. `name_bytes`
excludes NUL; read-only `name_storage` includes all NUL bytes from c_namesize.
Multiple trailing NUL name bytes are accepted; interior NUL followed by nonzero
name data is rejected. A trailer must have zero payload.

The reader does not extract, resolve hardlinks, normalize names or enforce a
filesystem policy. Consumers must interpret `(dev_major, dev_minor, ino)` with
`nlink`, including data on a later hardlink. Stop at the trailer for one archive;
use initsysutils for concatenated Linux initramfs members. Bytes following a
trailer belong to the enclosing container.

## Construction and ownership

Initialize `mulib_cpio_writer_t` with `writer_init(buffer, capacity, format)`.
Populate a zero-initialized entry; name_bytes/data size are explicit and source
data is borrowed only during `add`. The writer computes checksums and a canonical
single-NUL name; the input checksum/format/name_storage fields are ignored.
Set nlink, inode and mode deliberately. `record_size` estimates padded size.
`finish` adds the trailer and pads to a caller-selected power of two: 512 for
conventional cpio output or 4 for compact initramfs. Adding after finish fails.
All capacity/arithmetic failures are atomic: no partial output or cursor change.
Input and output storage must not overlap; writers must not be mutated manually.

Limits are the format's 32-bit fields and host size_t/capacity. Name bytes must
be nonempty and leave room for NUL in UINT32_MAX. There is no hidden allocation
or path limit in the generic archive layer. Work is linear in headers, names and
checksum data; archives with huge data still require checksum traversal.

Results: OK=0, TRAILER=1, INVALID=-1, FORMAT=-2, BOUNDS=-3, CHECKSUM=-4,
UNSUPPORTED=-5, FINISHED=-6. No output is an authentication guarantee.

## References and verification

[Linux initramfs buffer format](https://docs.kernel.org/driver-api/early-userspace/buffer-format.html)
and GNU cpio newc/crc conventions (accessed 2026-11-06). Independent tests
read GNU-generated newc/crc hardlinks, and GNU cpio lists/extracts archives
written here. Boundary, checksum and malformed-header tests live in
`tests/test_cpio_init_shoebox.c`.

GNU cpio emits zero instead of a data checksum for crc symlinks; this reader
applies the documented crc byte-sum rule to every payload and rejects such a
record when its sum is nonzero. Use GNU newc for symlink interoperability.
See also [cpio(5)](https://man.freebsd.org/cgi/man.cgi?query=cpio&sektion=5).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-cpio.h`](../include/mulib-cpio.h) |
| Implementation switch | `MULIB_IMPL_CPIO` |
| Repository-local includes | None |
| Complete example | [`examples/modules/cpio.c`](../examples/modules/cpio.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/cpio.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
