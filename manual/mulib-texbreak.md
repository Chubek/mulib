# mulib-texbreak

**Area:** Typesetting  
**Implementation:** `MULIB_IMPL_TEXBREAK` (define in exactly one C translation unit)  
**Priority:** P0  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Optimal dynamic-programming line breaks over fixed widths and penalties.

## API

```c
typedef struct mulib_texbreak_result { size_t *breaks;size_t count; } mulib_texbreak_result_t;
int mulib_texbreak_solve(const double *widths,const int *penalties,size_t count,double line_width,mulib_texbreak_result_t *out);
void mulib_texbreak_destroy(mulib_texbreak_result_t *out);
```

## Behavior and limitations

Quadratic-DP width-only segmentation; not yet full Knuth-Plass active nodes, stretch/shrink fitness classes or shape-aware breaking.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texbreak.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-texbreak.c -lm -o test
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
| Header | [`include/mulib-texbreak.h`](../include/mulib-texbreak.h) |
| Implementation switch | `MULIB_IMPL_TEXBREAK` |
| Repository-local includes | [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-texbreak.c`](../examples/mulib-texbreak.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-texbreak.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
