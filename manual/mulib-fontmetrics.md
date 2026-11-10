# mulib-fontmetrics

**Area:** Font and Unicode  
**Implementation:** `MULIB_IMPL_FONTMETRICS` (define in exactly one C translation unit)  
**Priority:** P0  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

HMetrics width lookup from caller-extracted hmtx table and hhea count.

## API

```c
int mulib_fontmetrics_advance(const void *hmtx,size_t bytes,uint16_t long_metrics,uint16_t glyph,uint16_t *advance,uint16_t *lsb);
```

## Behavior and limitations

Only horizontal advances and left-side bearings. Vertical metrics and variation deltas are separate.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-fontmetrics.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-fontmetrics.c -lm -o test
./test
```

## Hardening checklist

- Add boundary, allocation-failure, overflow and malformed-input tests.
- Compare with a mature reference implementation whenever the domain has a normative file format or algorithm.
- Preserve caller-owned vs callee-owned resources in documentation.
- Support deterministic results and failure-atomic writes on all APIs.
- Add integration fixtures for every dependent format and target.

## Signed bearings and atomic results

Horizontal long metrics and trailing bearings follow **OpenType 1.9.1** hmtx.
The legacy `advance` function retains its `uint16_t` bearing ABI (the raw signed
bit pattern). New `mulib_fontmetrics_signed` returns a correctly signed `int16_t`
bearing. Both validate the complete requested record before changing either
output, including glyphs that reuse the last long advance and read a trailing
bearing. The caller supplies a valid glyph index/count from maxp; these functions
do not infer the font's glyph count from padded table length. No hinting,
variation deltas, vertical metrics or outline bounds are supplied here.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-fontmetrics.h`](../include/mulib-fontmetrics.h) |
| Implementation switch | `MULIB_IMPL_FONTMETRICS` |
| Repository-local includes | [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-fontmetrics.c`](../examples/mulib-fontmetrics.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-fontmetrics.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
