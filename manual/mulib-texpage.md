# mulib-texpage

**Area:** Typesetting  
**Implementation:** `MULIB_IMPL_TEXPAGE` (define in exactly one C translation unit)  
**Priority:** P0  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Greedy bounded vertical page segmentation over box heights.

## API

```c
typedef struct mulib_texpage_pages { size_t *ends;size_t count; } mulib_texpage_pages_t;
int mulib_texpage_paginate(const double *heights,size_t count,double page_height,mulib_texpage_pages_t *out);
void mulib_texpage_destroy(mulib_texpage_pages_t *out);
```

## Behavior and limitations

Greedy break baseline; insertions, penalties, marks and page optimization are additional responsibilities.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texpage.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-texpage.c -lm -o test
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
| Header | [`include/mulib-texpage.h`](../include/mulib-texpage.h) |
| Implementation switch | `MULIB_IMPL_TEXPAGE` |
| Repository-local includes | [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-texpage.c`](../examples/mulib-texpage.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-texpage.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
