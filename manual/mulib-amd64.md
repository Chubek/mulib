# AMD64

[Manuals](README.md) · [Library index](index.md)

`mulib-amd64.h` owns both the original dsymspec typed assembler and an enriched
[MPU](mulib-mpu.md) ArchML profile. The original API retains its signatures and
return values; the MPU path adds concrete memory/SIMD forms, named decoding,
register/ABI metadata and explicit feature queries.

## Integration and lifecycle

Define `MULIB_IMPL_AMD64`, `MULIB_IMPL_MPU`, `MULIB_IMPL_UNIT`,
`MULIB_IMPL_ARCHML`, `MULIB_IMPL_ASSEMBLER`, `MULIB_IMPL_DSYMSPEC`,
`MULIB_IMPL_JSON`, `MULIB_IMPL_PARSEC`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_UTF8`,
`MULIB_IMPL_EMITTER` and `MULIB_IMPL_LINKER`. The MPU/UNIT/ARCHML switches are
new dependencies for existing AMD64 users. No switches are enabled implicitly.
See the [complete example](../examples/modules/amd64.c).

Zero initialize `mulib_amd64_t`; call `init` or `init_ex`, then `free`.
Both initializers return Boolean 1/0. `init_ex` refreshes an optional ArchML
diagnostic. Both owners are initialized atomically: failure releases temporary
allocations and leaves the supplied context unchanged. Live contexts are
rejected. Free resets both owners; NULL/repeated free is safe and permits reuse.
Do not copy live ownership or edit public fields. The context has grown to hold
the MPU profile; rebuild clients rather than assuming its old struct size.

## Two emission interfaces

`mulib_amd64_emit` retains typed register/immediate/symbol operands and the
[assembler](mulib-assembler.md) supported subset. **Success remains 0**
(`MULIB_ASSEMBLER_OK`). Symbolic branches use the supplied linker. Invalid
contexts now refresh the diagnostic and clear stale linker status. Assembler
failure leaves image and linker unchanged.

`mulib_amd64_mpu` borrows the initialized MPU contract, or returns NULL. Use
`mulib_mpu_document`, `find`, `register_info` and `enabled` through this view.
Metadata remains owned by the AMD64 context and is invalidated at free.

`mulib_amd64_encode`, `decode` and `emit_form` use **concrete ArchML form names**
and numeric `uint64_t` operands. Their success is **1** (`MULIB_ARCHML_OK`), with
negative ArchML errors. For example, `load64-base-disp32 {9,12,(uint64_t)-16}`
encodes `4d 8b 8c 24 f0 ff ff ff`. The embedded profile also provides selected
SSE/AVX and other extended forms; consult [coverage](../library/archml/README.md).
Features must be checked explicitly. Concrete forms do not resolve symbols.
`decode` validates a named form and consumes one prefix; it is not an automatic
disassembler. All codec outputs and emitter contents are unchanged on failure.

Both binary emission paths require a little-endian image. No host CPU discovery,
instruction execution or executable-memory allocation occurs. The
[unit contract](mulib-unit.md) documents operand ranges, parser limits,
allocation behavior and concurrency. Existing [storage/server tests](../tests/test_storage_server.c)
retain legacy API coverage; [unit tests](../tests/test_units.c) exercise the new
MPU path, diagnostics and transactional failures.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-amd64.h`](../include/mulib-amd64.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-amd64.h
 * @brief Owned AMD64 assembler and enriched MPU architecture context.
 * Define MULIB_IMPL_AMD64 and all ASSEMBLER and MPU dependencies. The legacy
 * typed assembler uses dsymspec; concrete-form codecs and queries use ArchML.
 * Zero initialize; free before reinitializing. Never copy ownership. */
#ifndef MULIB_AMD64_H
#define MULIB_AMD64_H
#include "mulib-assembler.h"
#include "mulib-mpu.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_amd64
  {
    mulib_dsymspec_t *specification;
    mulib_mpu_t mpu;
  } mulib_amd64_t;
  /** @brief Initialize both metadata owners atomically. Return 1/0. */
  int mulib_amd64_init (mulib_amd64_t *context);
  /** @brief Boolean initialization with a refreshed optional diagnostic. */
  int mulib_amd64_init_ex (mulib_amd64_t *context, mulib_archml_error_t *error);
  /** @brief Release/reset both owners. NULL/repeated free permitted. */
  void mulib_amd64_free (mulib_amd64_t *context);
  /** @brief Legacy typed mnemonic API: success is MULIB_ASSEMBLER_OK (0).
   * Preserves image/linker on failure and refreshes error even for bad context. */
  int mulib_amd64_emit (const mulib_amd64_t *context, mulib_emitter_t *image,
                        mulib_linker_t *linker, const char *mnemonic,
                        const mulib_assembler_operand_t *operands, size_t count,
                         mulib_assembler_error_t *error);
  /** @brief Borrow the MPU contract, or NULL when not initialized. */
  const mulib_mpu_t *mulib_amd64_mpu (const mulib_amd64_t *context);
  /** @brief Encode a concrete ArchML form; success is MULIB_ARCHML_OK (1).
   * Includes memory, SIMD and extended forms present in the profile. */
  mulib_archml_result mulib_amd64_encode (const mulib_amd64_t *, const char *,
      const uint64_t *, size_t, void *, size_t, size_t *);
  /** @brief Transactional named-form decoding; success is 1. */
  mulib_archml_result mulib_amd64_decode (const mulib_amd64_t *, const char *,
      const void *, size_t, uint64_t *, size_t, size_t *);
  /** @brief Append a concrete form with numeric operands; success is 1.
   * No labels/relocations; use legacy emit for symbolic branches. */
  mulib_archml_result mulib_amd64_emit_form (const mulib_amd64_t *,
      mulib_emitter_t *, const char *, const uint64_t *, size_t,
      mulib_archml_error_t *);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-amd64.h`](../include/mulib-amd64.h) |
| Implementation switch | `MULIB_IMPL_AMD64` |
| Repository-local includes | [mulib-assembler](mulib-assembler.md), [mulib-mpu](mulib-mpu.md) |
| Complete example | [`examples/modules/amd64.c`](../examples/modules/amd64.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/amd64.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
