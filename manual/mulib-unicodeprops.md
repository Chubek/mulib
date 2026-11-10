# mulib-unicodeprops

**Area:** Font and Unicode  
**Implementation:** `MULIB_IMPL_UNICODEPROPS` (define in exactly one C translation unit)  
**Priority:** P0  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Unicode scalar validity and conservative text class classification.

## API

```c
enum { mulib_unicodeprops_CONTROL=1,mulib_unicodeprops_SPACE=2,mulib_unicodeprops_MARK=3,mulib_unicodeprops_LETTER=4,mulib_unicodeprops_NUMBER=5,mulib_unicodeprops_OTHER=6 };
int mulib_unicodeprops_valid(uint32_t cp);
unsigned mulib_unicodeprops_class(uint32_t cp);
int mulib_unicodeprops_rtl(uint32_t cp);
```

## Behavior and limitations

Conservative subset only, NOT UCD-complete. Generate UCD tables before claiming Unicode conformance.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-unicodeprops.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-unicodeprops.c -lm -o test
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
| Header | [`include/mulib-unicodeprops.h`](../include/mulib-unicodeprops.h) |
| Implementation switch | `MULIB_IMPL_UNICODEPROPS` |
| Repository-local includes | [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-unicodeprops.c`](../examples/mulib-unicodeprops.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-unicodeprops.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
