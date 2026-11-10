# mulib-fontgdef

**Area:** Font and Unicode  
**Implementation:** `MULIB_IMPL_FONTGDEF` (define in exactly one C translation unit)  
**Priority:** P1  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Bounds-checked font and unicode fontgdef structures binary inspection and record access.

## API

```c
typedef struct mulib_fontgdef_view { mulib_typecore_slice_t bytes; } mulib_fontgdef_view_t;
int mulib_fontgdef_open(mulib_fontgdef_view_t *v, const void *bytes, size_t size);
int mulib_fontgdef_read_u16(const mulib_fontgdef_view_t *v, size_t offset, uint16_t *value);
int mulib_fontgdef_read_u32(const mulib_fontgdef_view_t *v, size_t offset, uint32_t *value);
int mulib_fontgdef_slice(const mulib_fontgdef_view_t *v, size_t offset, size_t length, mulib_typecore_slice_t *out);
```

## Behavior and limitations

Basic bounded binary substrate for font and unicode fontgdef structures; decoding all semantics/encodings is not implemented.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-fontgdef.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-fontgdef.c -lm -o test
./test
```

## Hardening checklist

- Add boundary, allocation-failure, overflow and malformed-input tests.
- Compare with a mature reference implementation whenever the domain has a normative file format or algorithm.
- Preserve caller-owned vs callee-owned resources in documentation.
- Support deterministic results and failure-atomic writes on all APIs.
- Add integration fixtures for every dependent format and target.

## Glyph classification

`mulib_fontgdef_glyph_class` reads the GDEF GlyphClassDef using class definition
formats 1/2 from **OpenType 1.9.1**. Values are 0 (unclassified), 1 (base),
2 (ligature), 3 (mark), or 4 (component); an absent class definition returns zero.
Invalid classes, glyphs above 65535 and truncated/unsupported structures fail
without changing the output. The input is a borrowed, immutable raw view.
This does not implement GDEF attachment points, ligature caret evaluation,
mark-attachment classes, mark filtering sets or variation stores. The native
Latin backend uses classification to reject mark glyphs, not to claim mark shaping.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-fontgdef.h`](../include/mulib-fontgdef.h) |
| Implementation switch | `MULIB_IMPL_FONTGDEF` |
| Repository-local includes | [`include/detail/mulib-otlayout.h`](../include/detail/mulib-otlayout.h), [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-fontgdef.c`](../examples/mulib-fontgdef.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-fontgdef.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
