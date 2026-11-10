# mulib-unicodebidi

**Area:** Font and Unicode  
**Implementation:** `MULIB_IMPL_UNICODEBIDI` (define in exactly one C translation unit)  
**Priority:** P0  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Conservative base-direction detection from Hebrew/Arabic and ASCII letters.

## API

```c
enum { mulib_unicodebidi_LTR=0,mulib_unicodebidi_RTL=1 };
int mulib_unicodebidi_base(const char *utf8,size_t length,unsigned *direction);
```

## Behavior and limitations

Only paragraph base-direction detection, not UAX #9 run resolution or reordering.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-unicodebidi.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-unicodebidi.c -lm -o test
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
| Header | [`include/mulib-unicodebidi.h`](../include/mulib-unicodebidi.h) |
| Implementation switch | `MULIB_IMPL_UNICODEBIDI` |
| Repository-local includes | [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-unicodebidi.c`](../examples/mulib-unicodebidi.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-unicodebidi.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
