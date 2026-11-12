# webextract-evidence

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Source-span anchored extraction evidence records and integrity hashes.

## Contract and supported profile

Make copies a nonempty field label and stores a checked half-open source
span plus FNV-1a hashes of the whole document and selected bytes. Offset/length
are bytes, and empty spans are valid. Verify checks bounds and both hashes;
any document change invalidates the record. The record owns no document data,
and make preserves output on failure. These noncryptographic fingerprints are
not authentication, collision resistance or proof that a field is factually true.

## Integration and ownership

Include `include/mulib-webextract-evidence.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_UTF8`, `MULIB_IMPL_WEBEXTRACT_EVIDENCE`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/webextract-evidence.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_extraction.c`](../tests/test_web_extraction.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-webextract-evidence.h`](../include/mulib-webextract-evidence.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-webextract-evidence.h
 * @brief Source-span anchored extraction evidence records and integrity
 * hashes. C11 single-header module. Define MULIB_IMPL_WEBEXTRACT_EVIDENCE in
 * exactly one C translation unit.
 *
 */
#ifndef MULIB_WEBEXTRACT_EVIDENCE_H
#define MULIB_WEBEXTRACT_EVIDENCE_H
#include "mulib-shylock-core.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_webextract_evidence
  {
    size_t start, length;
    uint64_t document_hash, span_hash;
    char field[64];
  } mulib_webextract_evidence_t;
  /** @brief See manual/mulib-webextract-evidence.md for this operation's
   * contract. */
  int mulib_webextract_evidence_make (const char *document,
                                      size_t document_length, size_t offset,
                                      size_t length, const char *field,
                                      mulib_webextract_evidence_t *out);
  /** @brief See manual/mulib-webextract-evidence.md for this operation's
   * contract. */
  int mulib_webextract_evidence_verify (
      const char *document, size_t document_length,
      const mulib_webextract_evidence_t *evidence);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_WEBEXTRACT_EVIDENCE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-webextract-evidence.h`](../include/mulib-webextract-evidence.h) |
| Implementation switch | `MULIB_IMPL_WEBEXTRACT_EVIDENCE` |
| Repository-local includes | [mulib-shylock-core](mulib-shylock-core.md) |
| Complete example | [`examples/modules/webextract-evidence.c`](../examples/modules/webextract-evidence.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/webextract-evidence.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
