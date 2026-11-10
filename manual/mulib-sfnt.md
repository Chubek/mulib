# mulib-sfnt

**Area:** Font and Unicode  
**Implementation:** `MULIB_IMPL_SFNT` (define in exactly one C translation unit)  
**Priority:** P0  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Bounds-checked SFNT/OpenType table directory inspection.

## API

```c
typedef struct mulib_sfnt_font { mulib_typecore_slice_t data;uint16_t tables; } mulib_sfnt_font_t;
int mulib_sfnt_open(mulib_sfnt_font_t *f,const void *data,size_t size);
int mulib_sfnt_table(const mulib_sfnt_font_t *f,const char tag[4],mulib_typecore_slice_t *out);
int mulib_sfnt_units_per_em(const mulib_sfnt_font_t *f,uint16_t *units);
```

## Behavior and limitations

Inspects table directory with bounds validation; table-specific semantic conformance is separate.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-sfnt.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-sfnt.c -lm -o test
./test
```

## Hardening checklist

- Add boundary, allocation-failure, overflow and malformed-input tests.
- Compare with a mature reference implementation whenever the domain has a normative file format or algorithm.
- Preserve caller-owned vs callee-owned resources in documentation.
- Support deterministic results and failure-atomic writes on all APIs.
- Add integration fixtures for every dependent format and target.

## Upgraded directory and collection reader

The bounded reader follows the SFNT/TTC directory layouts of **OpenType 1.9.1**.
`open` keeps its existing standalone-font ABI. The new `mulib_sfnt_open_face`
returns a borrowed `mulib_sfnt_face_t` for a standalone face at index zero or a
TTC version 1/2 face at the requested index; use `mulib_sfnt_face_table` because
collection table offsets are absolute within the original file. Named-instance
bits are not interpreted. Storage must remain immutable and alive while views are used.

Directories are capped at 4,096 tables/faces. Truncated directories, duplicate or
unsorted tags, out-of-file ranges, overlap within a face, and overlap with its
directory are rejected. Shared tables across collection faces remain allowed.
Directory overlap checking is bounded quadratic work in the table count.
Table access rechecks bounds; failed opens/lookups and invalid units-per-em leave
outputs unchanged. Checksums, outlines, WOFF/WOFF2 decoding, and complete font
validity are not established by this reader. `tests/test-sfnt.c` and the native
shaping fixture cover malformed directories and collection offsets.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-sfnt.h`](../include/mulib-sfnt.h) |
| Implementation switch | `MULIB_IMPL_SFNT` |
| Repository-local includes | [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-sfnt.c`](../examples/mulib-sfnt.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-sfnt.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
