# mulib-typecontext

**Area:** Infrastructure  
**Implementation:** `MULIB_IMPL_TYPECONTEXT` (define in exactly one C translation unit)  
**Priority:** P0  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Reusable context owning a document working byte arena.

## API

```c
typedef struct mulib_typecontext_context {mulib_typecore_buf_t scratch;size_t max_bytes;} mulib_typecontext_context_t;
void mulib_typecontext_init(mulib_typecontext_context_t *ctx,size_t maximum);
int mulib_typecontext_append(mulib_typecontext_context_t *ctx,const void *bytes,size_t size);
void mulib_typecontext_reset(mulib_typecontext_context_t *ctx);
void mulib_typecontext_destroy(mulib_typecontext_context_t *ctx);
```

## Behavior and limitations

Baseline bounded functionality; this is not a complete application-level engine.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-typecontext.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-typecontext.c -lm -o test
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
| Header | [`include/mulib-typecontext.h`](../include/mulib-typecontext.h) |
| Implementation switch | `MULIB_IMPL_TYPECONTEXT` |
| Repository-local includes | [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-typecontext.c`](../examples/mulib-typecontext.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-typecontext.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
