# mulib-docvalidate

**Area:** Document IR  
**Implementation:** `MULIB_IMPL_DOCVALIDATE` (define in exactly one C translation unit)  
**Priority:** P0  
**Status:** usable baseline, NOT necessarily standards complete

## Purpose

Configurable execution policy and accounting for document ir docvalidate.

## API

```c
typedef struct mulib_docvalidate_policy { size_t limit,used; } mulib_docvalidate_policy_t;
void mulib_docvalidate_init(mulib_docvalidate_policy_t *policy, size_t limit);
int mulib_docvalidate_charge(mulib_docvalidate_policy_t *policy, size_t amount);
void mulib_docvalidate_reset(mulib_docvalidate_policy_t *policy);
size_t mulib_docvalidate_remaining(const mulib_docvalidate_policy_t *policy);
```

## Behavior and limitations

Accounting primitive, not a complete sandbox or a preemptive execution scheduler.

The public API validates arguments and most size/range boundaries; check integer and memory errors, and do not assume advanced standard conformance. For untrusted files or documents, add fuzzing and independent-format validation before production deployment.

## Integrating with MuLIB

The header includes `mulib-typecore.h`, which provides allocation-checked byte buffers and portable endian helpers. No third-party runtime library is needed for these baseline functions. Define `MULIB_IMPL_TYPECORE` once along with one implementation switch for each used module; additional translation units should include declarations only.

Build the standalone example:

```sh
cc -std=c11 -Wall -Wextra -Iinclude examples/mulib-docvalidate.c -lm -o demo
./demo
```

Run its test:

```sh
cc -std=c11 -Wall -Wextra -Iinclude tests/test-docvalidate.c -lm -o test
./test
```

## Hardening checklist

- Add boundary, allocation-failure, overflow and malformed-input tests.
- Compare with a mature reference implementation whenever the domain has a normative file format or algorithm.
- Preserve caller-owned vs callee-owned resources in documentation.
- Support deterministic results and failure-atomic writes on all APIs.
- Add integration fixtures for every dependent format and target.

## Whole-tree validation

Enable `MULIB_ENABLE_DOCIR` and the docir/typecore implementations for
`mulib_docvalidate_tree(root,limits,report)`. It checks complete-root ownership,
structural grammar, math arities, styles/source metadata, budgets, duplicate IDs,
dangling local references and bibliography citation targets. It is not TeX,
HTML5, EPUB or Unicode standards certification. The old policy accounting API
remains independent. See [the pipeline contract](docir-publishing.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-docvalidate.h`](../include/mulib-docvalidate.h) |
| Implementation switch | `MULIB_IMPL_DOCVALIDATE` |
| Repository-local includes | [mulib-docir](mulib-docir.md), [mulib-typecore](mulib-typecore.md) |
| Complete example | [`examples/mulib-docvalidate.c`](../examples/mulib-docvalidate.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-docvalidate.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
