# mulib-texglue

**Area:** Typesetting  
**Implementation:** `MULIB_IMPL_TEXGLUE` (define in exactly one C translation unit)  
**Priority:** P0  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

TeX-like natural size, stretch and shrink plus proportional adjustment.

## API

```c
typedef struct mulib_texglue_glue { int64_t natural,stretch,shrink;unsigned stretch_order,shrink_order; } mulib_texglue_glue_t;
int mulib_texglue_adjust(const mulib_texglue_glue_t *items,size_t count,int64_t desired,int64_t *sizes);
int mulib_texglue_sum(const mulib_texglue_glue_t *items,size_t count,int64_t *natural);
```

## Behavior and limitations

Fractional adjustments round toward zero; production engine must distribute residual scaled points deterministically.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texglue.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-texglue.c -lm -o test
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
| Header | [`include/mulib-texglue.h`](../include/mulib-texglue.h) |
| Implementation switch | `MULIB_IMPL_TEXGLUE` |
| Repository-local includes | [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-texglue.c`](../examples/mulib-texglue.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-texglue.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
