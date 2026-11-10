# mulib-typestatus

**Area:** Infrastructure  
**Implementation:** `MULIB_IMPL_TYPESTATUS` (define in exactly one C translation unit)  
**Priority:** P0  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Stable error codes and human-readable diagnostics for publishing modules.

## API

```c
typedef enum mulib_typestatus_code { mulib_typestatus_OK=0,mulib_typestatus_INVALID=-1,mulib_typestatus_NOMEM=-2,mulib_typestatus_LIMIT=-3,mulib_typestatus_UNSUPPORTED=-4,mulib_typestatus_FORMAT=-5 } mulib_typestatus_code_t;
const char *mulib_typestatus_name(mulib_typestatus_code_t status);
```

## Behavior and limitations

Baseline bounded functionality; this is not a complete application-level engine.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-typestatus.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-typestatus.c -lm -o test
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
| Header | [`include/mulib-typestatus.h`](../include/mulib-typestatus.h) |
| Implementation switch | `MULIB_IMPL_TYPESTATUS` |
| Repository-local includes | [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-typestatus.c`](../examples/mulib-typestatus.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-typestatus.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
