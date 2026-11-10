# mulib-typesandbox

**Area:** Infrastructure  
**Implementation:** `MULIB_IMPL_TYPESANDBOX` (define in exactly one C translation unit)  
**Priority:** P0  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Configurable execution policy and accounting for infrastructure typesandbox.

## API

```c
typedef struct mulib_typesandbox_policy { size_t limit,used; } mulib_typesandbox_policy_t;
void mulib_typesandbox_init(mulib_typesandbox_policy_t *policy, size_t limit);
int mulib_typesandbox_charge(mulib_typesandbox_policy_t *policy, size_t amount);
void mulib_typesandbox_reset(mulib_typesandbox_policy_t *policy);
size_t mulib_typesandbox_remaining(const mulib_typesandbox_policy_t *policy);
```

## Behavior and limitations

Accounting primitive, not a complete sandbox or a preemptive execution scheduler.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-typesandbox.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-typesandbox.c -lm -o test
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
| Header | [`include/mulib-typesandbox.h`](../include/mulib-typesandbox.h) |
| Implementation switch | `MULIB_IMPL_TYPESANDBOX` |
| Repository-local includes | [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-typesandbox.c`](../examples/mulib-typesandbox.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-typesandbox.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
