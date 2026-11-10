# mulib-texexpand

**Area:** Language  
**Implementation:** `MULIB_IMPL_TEXEXPAND` (define in exactly one C translation unit)  
**Priority:** P0  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Simple bounded macro-name substitution with single-pass escaped backslash commands.

## API

```c
typedef struct mulib_texexpand_macro { char name[64];char *replacement; } mulib_texexpand_macro_t;
typedef struct mulib_texexpand_engine { mulib_texexpand_macro_t *items;size_t count,capacity; } mulib_texexpand_engine_t;
int mulib_texexpand_define(mulib_texexpand_engine_t *engine,const char *name,const char *replacement);
int mulib_texexpand_once(const mulib_texexpand_engine_t *engine,const char *source,mulib_typecore_buf_t *out);
void mulib_texexpand_destroy(mulib_texexpand_engine_t *engine);
```

## Behavior and limitations

Single-pass simple replacement only. Full TeX macro argument scanning, expansion order and catcode semantics remain work items.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texexpand.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-texexpand.c -lm -o test
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
| Header | [`include/mulib-texexpand.h`](../include/mulib-texexpand.h) |
| Implementation switch | `MULIB_IMPL_TEXEXPAND` |
| Repository-local includes | [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-texexpand.c`](../examples/mulib-texexpand.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-texexpand.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
