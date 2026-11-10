# mulib-fontgsub

**Area:** Font and Unicode  
**Implementation:** `MULIB_IMPL_FONTGSUB` (define in exactly one C translation unit)  
**Priority:** P0  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Checked GSUB views and bounded single/ligature substitution execution.

## API

```c
typedef struct mulib_fontgsub_view { mulib_typecore_slice_t bytes; } mulib_fontgsub_view_t;
int mulib_fontgsub_open(mulib_fontgsub_view_t *v, const void *bytes, size_t size);
int mulib_fontgsub_read_u16(const mulib_fontgsub_view_t *v, size_t offset, uint16_t *value);
int mulib_fontgsub_read_u32(const mulib_fontgsub_view_t *v, size_t offset, uint32_t *value);
int mulib_fontgsub_slice(const mulib_fontgsub_view_t *v, size_t offset, size_t length, mulib_typecore_slice_t *out);
```

## Behavior and limitations

The raw view/accessor API is retained. `open` creates a borrowed binary view, not
a certificate that the entire table is valid. `substitute(view,lookup,glyphs,count,
&replacement,&consumed)` now executes a lookup at the start of a caller-verified
mark-free glyph sequence using the **OpenType 1.9.1** layouts:

- Lookup 1, single substitution formats 1/2.
- Lookup 4, ligature substitution format 1, respecting font-defined candidate order.
- Lookup 7, one extension level to those types (recursive extensions fail).

Coverage formats 1/2 are read with bounded searches. Flags zero and IgnoreMarks
are accepted only for a mark-free input sequence; this helper does not filter
GDEF marks itself. The native fontshape caller checks GDEF before using it.
A non-match succeeds with the original first glyph and `consumed=1`; a ligature
reports the number of components consumed. Inputs are not modified and result
outputs are unchanged on failure. Empty input, unsupported flags/types, bad
offsets and incomplete records fail. Each call has a 65,536-step subtable/candidate/
component budget, including for aliased ligature records. Contextual, chained, alternate, multiple,
reverse, mark-filtering and script-specific shaping are not implemented here.
The caller selects features and maintains clusters; see [native fontshape](mulib-fontshape.md#native-latin-implementation)
and `tests/test-font-layout.c`. No complete GSUB/shaping conformance is claimed.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-fontgsub.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-fontgsub.c -lm -o test
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
| Header | [`include/mulib-fontgsub.h`](../include/mulib-fontgsub.h) |
| Implementation switch | `MULIB_IMPL_FONTGSUB` |
| Repository-local includes | [`include/detail/mulib-otlayout.h`](../include/detail/mulib-otlayout.h), [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-fontgsub.c`](../examples/mulib-fontgsub.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-fontgsub.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
