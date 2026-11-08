# MuLIB artifact

[Manuals](README.md) · [Library index](index.md)

**Header:** `include/mulib-artifact.h`  
**Switch:** `MULIB_IMPL_ARTIFACT` · **Dependencies:** none

Artifacts are immutable envelopes for application values. They carry a required
application identity and type, an optional media type and locator, and one of:

* `MULIB_ARTIFACT_BYTES`: an owned copy of arbitrary bytes, including embedded NULs;
* `MULIB_ARTIFACT_REFERENCE`: an external reference with a required nonempty locator;
* `MULIB_ARTIFACT_OBJECT`: an opaque application object with an optional destructor.

Identities and types are uninterpreted strings. They need not be filenames,
content hashes, build products, or globally unique. Messages, documents, tool
results and agent-session objects can use the same API as a build graph.
References are descriptive: creation does not resolve a URI or open a file.

## Ownership

Zero-initialize `mulib_artifact_desc_t`, fill its fields, and call
`mulib_artifact_create`. All strings and byte payloads are copied. An object
destructor transfers ownership **only when creation succeeds**; without a
destructor, the application keeps ownership and must keep the object alive.
The descriptor's object fields must be empty for other kinds, and its byte fields
must be empty except for BYTES. An OBJECT requires a non-NULL object.

`mulib_artifact_view` borrows a read-only descriptor until the last release.
The descriptor cannot be mutated. An opaque object's internal mutability remains
the application's responsibility; build inputs should be immutable snapshots.
`retain` adds a reference, `release` removes one, and the final release frees
the copied storage and invokes the object destructor exactly once. Reference
counts are atomic; each thread must already own a reference before retaining or
viewing. This does not synchronize mutable opaque objects. `retain` returns
NULL on NULL or count overflow; `release(NULL)` is harmless.

`create` returns NULL for invalid descriptors, byte lengths greater than
`PTRDIFF_MAX`, or allocation failure. It has no partial ownership side effects.
Zero-length bytes are valid, including a NULL data pointer. There is no automatic
serialization of opaque objects, content equality, or URI canonicalization.

## Binary files and archives

`mulib_artifact_pack(id, files, count)` copies up to 65536
`mulib_artifact_file_t` entries into a BYTES artifact of type `mulib.archive.v1`.
Each entry has a canonical relative slash path (1..4095 bytes), binary contents,
permission bits (0..0777), a uint64 Unix-seconds mtime, and FILE or DIFF kind.
Duplicates, absolute/traversal paths, empty components, colons, backslashes and
control bytes are rejected. DIFF payload encoding is application-defined; BuildVM
produces VCDIFF bytes. No symlinks, directories, owners, xattrs or host I/O are
implicitly represented. Supply metadata explicitly; zero timestamps make
reproducible archives easy. A one-file archive uses the same API.

Wire format (all integers unsigned little-endian uint64):

* 24-byte header: 8 magic/version bytes `MUART\0\1\0`, entry count, total length.
* `count` 48-byte TOC records: name offset, name length excluding NUL, payload
  offset, payload length, mtime, flags (`mode | kind << 9`).
* In TOC order, each NUL-terminated name followed immediately by its payload.

Offsets are absolute from the first header byte. Empty entries/archives work.
Canonical placement forbids overlaps, gaps, trailing bytes and hidden records;
unknown flag bits are rejected. For a fixed entry order, contents and metadata,
encoding is deterministic. There is no compression, checksum, authentication or
implicit content hashing. Applications must authenticate untrusted storage.

`unpack(id, data, length)` validates the entire buffer before copying it.
`file_count` validates a BYTES artifact and writes its count; `file_at` returns a
borrowed path/data view plus metadata. Both return 0 or -1 and leave outputs
unchanged on error. Pack/unpack return NULL on invalid input or allocation failure.
Validation checks names pairwise: use bounded archives and reject oversized input
buffers before calling when applications need tighter resource limits.
`tests/test_build_tasks.c` covers truncations, offsets, paths, metadata, empty
archives and binary round trips.

## Example and verification

```sh
cc -std=c11 -Iinclude examples/modules/artifact.c -o /tmp/opencode/artifact
/tmp/opencode/artifact
```

`tests/test_buildgraph.c` checks binary payload copying, empty values, references,
destructor ownership, invalid descriptors and allocation failures, together with
retained graph inputs and outputs.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-artifact.h`](../include/mulib-artifact.h) |
| Implementation switch | `MULIB_IMPL_ARTIFACT` |
| Repository-local includes | None |
| Complete example | [`examples/modules/artifact.c`](../examples/modules/artifact.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/artifact.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
