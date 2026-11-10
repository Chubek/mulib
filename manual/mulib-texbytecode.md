# mulib-texbytecode

**Area:** Language  
**Implementation:** `MULIB_IMPL_TEXBYTECODE` (define in exactly one C translation unit)  
**Priority:** P0  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Safe stack-bytecode arithmetic interpreter with bounded instruction steps.

## API

```c
enum { mulib_texbytecode_PUSH=1,mulib_texbytecode_ADD=2,mulib_texbytecode_SUB=3,mulib_texbytecode_MUL=4,mulib_texbytecode_DIV=5,mulib_texbytecode_HALT=6 };
typedef struct mulib_texbytecode_insn { unsigned opcode;double literal; } mulib_texbytecode_insn_t;
int mulib_texbytecode_run(const mulib_texbytecode_insn_t *program,size_t length,size_t max_steps,double *result);
```

## Behavior and limitations

Scalar arithmetic VM with finite step/stack limits; does not implement closures, objects or host functions.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-texbytecode.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-texbytecode.c -lm -o test
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
| Header | [`include/mulib-texbytecode.h`](../include/mulib-texbytecode.h) |
| Implementation switch | `MULIB_IMPL_TEXBYTECODE` |
| Repository-local includes | [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-texbytecode.c`](../examples/mulib-texbytecode.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-texbytecode.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
