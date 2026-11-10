# mulib-texvalue

**Area:** Language  
**Implementation:** `MULIB_IMPL_TEXVALUE` (define in exactly one C translation unit)  
**Priority:** P0  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Owned number/string value container with safe copy and destruction.

## API

```c
enum { mulib_texvalue_NIL=0,mulib_texvalue_NUMBER=1,mulib_texvalue_STRING=2 };
typedef struct mulib_texvalue_value { unsigned tag;double number;char *string; } mulib_texvalue_value_t;
int mulib_texvalue_string(mulib_texvalue_value_t *out,const char *str);
int mulib_texvalue_number(mulib_texvalue_value_t *out,double number);
int mulib_texvalue_copy(mulib_texvalue_value_t *out,const mulib_texvalue_value_t *src);
void mulib_texvalue_destroy(mulib_texvalue_value_t *value);
```

## Behavior and limitations

Number and owned string values only. Dynamic lists/maps/closures are planned extensions.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texvalue.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-texvalue.c -lm -o test
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
| Header | [`include/mulib-texvalue.h`](../include/mulib-texvalue.h) |
| Implementation switch | `MULIB_IMPL_TEXVALUE` |
| Repository-local includes | [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-texvalue.c`](../examples/mulib-texvalue.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-texvalue.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
