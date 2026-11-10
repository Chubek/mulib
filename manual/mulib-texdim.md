# mulib-texdim

**Area:** Typesetting  
**Implementation:** `MULIB_IMPL_TEXDIM` (define in exactly one C translation unit)  
**Priority:** P0  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

TeX-compatible scaled-point integer representation and overflow-checked arithmetic.

## API

```c
typedef int64_t mulib_texdim_sp_t;
#define MULIB_TEXDIM_SP_PER_PT 65536LL
int mulib_texdim_from_pt(double points,mulib_texdim_sp_t *out);
double mulib_texdim_to_pt(mulib_texdim_sp_t value);
int mulib_texdim_add(mulib_texdim_sp_t a,mulib_texdim_sp_t b,mulib_texdim_sp_t *out);
```

## Behavior and limitations

Baseline bounded functionality; this is not a complete application-level engine.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texdim.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-texdim.c -lm -o test
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
| Header | [`include/mulib-texdim.h`](../include/mulib-texdim.h) |
| Implementation switch | `MULIB_IMPL_TEXDIM` |
| Repository-local includes | [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-texdim.c`](../examples/mulib-texdim.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-texdim.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
