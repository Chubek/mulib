# mulib-fontkern

**Area:** Font and Unicode  
**Implementation:** `MULIB_IMPL_FONTKERN` (define in exactly one C translation unit)  
**Priority:** P0  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Simple legacy TrueType kern format 0 pair lookup.

## API

```c
int mulib_fontkern_pair(const void *subtable,size_t length,uint16_t left,uint16_t right,int16_t *value);
```

## Behavior and limitations

Expects caller-extracted format 0 subtable; GPOS kerning is a different table format.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-fontkern.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-fontkern.c -lm -o test
./test
```

## Hardening checklist

- Add boundary, allocation-failure, overflow and malformed-input tests.
- Compare with a mature reference implementation whenever the domain has a normative file format or algorithm.
- Preserve caller-owned vs callee-owned resources in documentation.
- Support deterministic results and failure-atomic writes on all APIs.
- Add integration fixtures for every dependent format and target.

## Checked legacy subtable profile

This API reads a **TrueType/OpenType 1.9.1 legacy kern version-0, format-0
subtable**, including its six-byte subtable header (not the top-level kern table).
The declared length must cover the 14-byte header/search fields and all pairs.
Unsupported format/version, zero/truncated declared length and excessive pair
counts fail without changing the result. Signed values are decoded explicitly;
a missing pair returns zero. Pair records must be sorted as specified for the
binary search. Apple version-1 kern, GPOS, contextual/state-machine kerning and
vertical/cross-stream policy are not executed by this single-pair reader.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-fontkern.h`](../include/mulib-fontkern.h) |
| Implementation switch | `MULIB_IMPL_FONTKERN` |
| Repository-local includes | [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-fontkern.c`](../examples/mulib-fontkern.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-fontkern.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
