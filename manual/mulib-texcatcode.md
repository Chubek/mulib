# mulib-texcatcode

**Area:** Language  
**Implementation:** `MULIB_IMPL_TEXCATCODE` (define in exactly one C translation unit)  
**Priority:** P0  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Configurable ASCII TeX-like category codes (initialization and assignment).

## API

```c
enum { mulib_texcatcode_ESCAPE=0,mulib_texcatcode_BEGIN=1,mulib_texcatcode_END=2,mulib_texcatcode_SPACE=10,mulib_texcatcode_LETTER=11,mulib_texcatcode_OTHER=12,mulib_texcatcode_COMMENT=14 };
typedef struct mulib_texcatcode_table { unsigned char category[256]; } mulib_texcatcode_table_t;
void mulib_texcatcode_init(mulib_texcatcode_table_t *table);
void mulib_texcatcode_set(mulib_texcatcode_table_t *table,unsigned byte,unsigned category);
unsigned mulib_texcatcode_get(const mulib_texcatcode_table_t *table,unsigned byte);
```

## Behavior and limitations

Baseline bounded functionality; this is not a complete application-level engine.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texcatcode.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-texcatcode.c -lm -o test
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
| Header | [`include/mulib-texcatcode.h`](../include/mulib-texcatcode.h) |
| Implementation switch | `MULIB_IMPL_TEXCATCODE` |
| Repository-local includes | [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-texcatcode.c`](../examples/mulib-texcatcode.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-texcatcode.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
