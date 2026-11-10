# mulib-typefuzz

**Area:** Infrastructure  
**Implementation:** `MULIB_IMPL_TYPEFUZZ` (define in exactly one C translation unit)  
**Priority:** P2  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Configurable execution policy and accounting for infrastructure typefuzz.

## API

```c
typedef struct mulib_typefuzz_policy { size_t limit,used; } mulib_typefuzz_policy_t;
void mulib_typefuzz_init(mulib_typefuzz_policy_t *policy, size_t limit);
int mulib_typefuzz_charge(mulib_typefuzz_policy_t *policy, size_t amount);
void mulib_typefuzz_reset(mulib_typefuzz_policy_t *policy);
size_t mulib_typefuzz_remaining(const mulib_typefuzz_policy_t *policy);
```

## Behavior and limitations

Accounting primitive, not a complete sandbox or a preemptive execution scheduler.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-typefuzz.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-typefuzz.c -lm -o test
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
| Header | [`include/mulib-typefuzz.h`](../include/mulib-typefuzz.h) |
| Implementation switch | `MULIB_IMPL_TYPEFUZZ` |
| Repository-local includes | [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-typefuzz.c`](../examples/mulib-typefuzz.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-typefuzz.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
