# dom-mutation

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Bounded record journal for DOM mutation events.

## Contract and supported profile

A zero-initialized journal owns copied mutation records. Push accepts the
four enum kinds and a nonzero node ID, assigns a monotonically increasing
sequence, and honors the optional record limit (zero unlimited). Sequence wrap,
invalid input and allocation failure leave state unchanged. Parent zero denotes
no parent. This does not perform tree edits or dispatch observers. Clear frees
storage and resets sequence and limit; array views expire on growth/clear.

## Integration and ownership

Include `include/mulib-dom-mutation.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_DOM_MUTATION`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/dom-mutation.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_browser.c`](../tests/test_web_browser.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-dom-mutation.h`](../include/mulib-dom-mutation.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-dom-mutation.h
 * @brief Bounded record journal for DOM mutation events.
 * C11 single-header module. Define MULIB_IMPL_DOM_MUTATION in exactly one C
 * translation unit.
 *
 */
#ifndef MULIB_DOM_MUTATION_H
#define MULIB_DOM_MUTATION_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef enum mulib_dom_mutation_kind
  {
    MULIB_DOM_MUTATION_INSERT = 1,
    MULIB_DOM_MUTATION_REMOVE = 2,
    MULIB_DOM_MUTATION_TEXT = 3,
    MULIB_DOM_MUTATION_ATTRIBUTE = 4
  } mulib_dom_mutation_kind_t;
  typedef struct mulib_dom_mutation_record
  {
    mulib_dom_mutation_kind_t kind;
    uint64_t node_id, parent_id, sequence;
  } mulib_dom_mutation_record_t;
  typedef struct mulib_dom_mutation_journal
  {
    mulib_dom_mutation_record_t *records;
    size_t len, cap, limit;
    uint64_t sequence;
  } mulib_dom_mutation_journal_t;
  /** @brief See manual/mulib-dom-mutation.md for this operation's contract. */
  int mulib_dom_mutation_push (mulib_dom_mutation_journal_t *j,
                               mulib_dom_mutation_kind_t kind, uint64_t node,
                               uint64_t parent);
  /** @brief See manual/mulib-dom-mutation.md for this operation's contract. */
  void mulib_dom_mutation_clear (mulib_dom_mutation_journal_t *j);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_DOM_MUTATION_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-dom-mutation.h`](../include/mulib-dom-mutation.h) |
| Implementation switch | `MULIB_IMPL_DOM_MUTATION` |
| Repository-local includes | None |
| Complete example | [`examples/modules/dom-mutation.c`](../examples/modules/dom-mutation.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/dom-mutation.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
