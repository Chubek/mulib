# mulib-texmouth

**Area:** Language  
**Implementation:** `MULIB_IMPL_TEXMOUTH` (define in exactly one C translation unit)  
**Priority:** P0  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

TeX-like borrowed token scanning: control words, braces, spaces and comments.

## API

```c
typedef enum mulib_texmouth_kind { mulib_texmouth_END_OF_INPUT=0,mulib_texmouth_TEXT=1,mulib_texmouth_CONTROL=2,mulib_texmouth_OPEN=3,mulib_texmouth_CLOSE=4,mulib_texmouth_WHITESPACE=5,mulib_texmouth_COMMENT=6 } mulib_texmouth_kind_t;
typedef struct mulib_texmouth_token { mulib_texmouth_kind_t kind;size_t offset,length; } mulib_texmouth_token_t;
typedef struct mulib_texmouth_scanner { const char *source;size_t size,position; } mulib_texmouth_scanner_t;
int mulib_texmouth_init(mulib_texmouth_scanner_t *s,const char *input,size_t length);
int mulib_texmouth_next(mulib_texmouth_scanner_t *s,mulib_texmouth_token_t *token);
```

## Behavior and limitations

Fixed ASCII lexical profile: not yet full TeX catcode transitions or Unicode control words.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texmouth.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-texmouth.c -lm -o test
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
| Header | [`include/mulib-texmouth.h`](../include/mulib-texmouth.h) |
| Implementation switch | `MULIB_IMPL_TEXMOUTH` |
| Repository-local includes | [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-texmouth.c`](../examples/mulib-texmouth.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-texmouth.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
