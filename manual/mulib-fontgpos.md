# mulib-fontgpos

**Area:** Font and Unicode  
**Implementation:** `MULIB_IMPL_FONTGPOS` (define in exactly one C translation unit)  
**Priority:** P0  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Bounds-checked font and unicode fontgpos structures binary inspection and record access.

## API

```c
typedef struct mulib_fontgpos_view { mulib_typecore_slice_t bytes; } mulib_fontgpos_view_t;
int mulib_fontgpos_open(mulib_fontgpos_view_t *v, const void *bytes, size_t size);
int mulib_fontgpos_read_u16(const mulib_fontgpos_view_t *v, size_t offset, uint16_t *value);
int mulib_fontgpos_read_u32(const mulib_fontgpos_view_t *v, size_t offset, uint32_t *value);
int mulib_fontgpos_slice(const mulib_fontgpos_view_t *v, size_t offset, size_t length, mulib_typecore_slice_t *out);
```

## Behavior and limitations

The original borrowed view/accessors remain available. The additive `pair` API
executes **OpenType 1.9.1** pair positioning lookup 2, formats 1 (explicit pairs)
and 2 (class pairs), plus lookup 9 extension to lookup 2. Coverage and class
definition formats 1/2 are handled with checked ranges. It returns first/second
glyph x/y placement and advance deltas in design units; a non-match returns zero
deltas. `consumed` is 2 for a matched nonempty second ValueRecord, otherwise 1,
so callers preserve OpenType pair traversal semantics.

Only scalar ValueRecords and flags zero/IgnoreMarks are accepted; the caller
must supply an already verified mark-free pair. Device/variation fields,
recursive extensions, other lookup types, GDEF filtering, cursive/mark attachment
and contextual positioning fail or remain outside this API. Outputs are unchanged
on failure, and no storage is allocated. `open` alone is only a raw view; it does
not validate all table semantics or prove OpenType conformance. Execution is tested
in `tests/test-font-layout.c` and the native fontshape differential suite.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-fontgpos.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-fontgpos.c -lm -o test
./test
```

## Hardening checklist

- Add boundary, allocation-failure, overflow and malformed-input tests.
- Compare with a mature reference implementation whenever the domain has a normative file format or algorithm.
- Preserve caller-owned vs callee-owned resources in documentation.
- Support deterministic results and failure-atomic writes on all APIs.
- Add integration fixtures for every dependent format and target.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-fontgpos.h`](../include/mulib-fontgpos.h) |
| Implementation switch | `MULIB_IMPL_FONTGPOS` |
| Repository-local includes | [`include/detail/mulib-otlayout.h`](../include/detail/mulib-otlayout.h), [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-fontgpos.c`](../examples/mulib-fontgpos.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-fontgpos.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
