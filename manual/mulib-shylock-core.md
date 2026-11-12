# shylock-core

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Bounded owned byte buffers, slices, validated parsing, hashing and serialization helpers.

## Contract and supported profile

Buffers are zero-initialized, own their storage, and count payload bytes, not
the trailing NUL. `reserve` and `append` preserve contents on failure and permit
bounded self-appends across moving reallocations. `text` requires a C string;
`json_string` validates UTF-8 and appends a complete escaped JSON string atomically.
Zero limit means unlimited; SIZE_MAX is not a representable payload length.
`dup` returns malloc-owned terminated bytes, `copy` rejects truncation, and free
resets all configuration. Hashing is noncryptographic FNV-1a (NULL/nonzero length
returns zero). Case-insensitive helpers compare byte strings. Decimal parsing
accepts signed decimal/fraction/exponent syntax and surrounding whitespace,
rejecting hexadecimal, nonfinite, malformed and unrepresentable values.

## Integration and ownership

Include `include/mulib-shylock-core.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_UTF8`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/shylock-core.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_extraction.c`](../tests/test_web_extraction.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-shylock-core.h`](../include/mulib-shylock-core.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-shylock-core.h
 * @brief Bounded owned byte buffers, slices, validated parsing, hashing and
 * serialization helpers. C11 single-header module. Define
 * MULIB_IMPL_SHYLOCK_CORE in exactly one C translation unit.
 *
 */
#ifndef MULIB_SHYLOCK_CORE_H
#define MULIB_SHYLOCK_CORE_H
#include "mulib-utf8.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct mulib_shylock_buffer
  {
    char *data;
    size_t len, cap, limit;
  } mulib_shylock_buffer_t;
  typedef struct mulib_shylock_span
  {
    const char *data;
    size_t len;
  } mulib_shylock_span_t;
  /** @brief Reserve payload bytes (terminator excluded); zero limit is
   * unlimited. */
  int mulib_shylock_reserve (mulib_shylock_buffer_t *b, size_t n);
  /** @brief Append copied bytes, including self-slices; failure preserves
   * contents. */
  int mulib_shylock_append (mulib_shylock_buffer_t *b, const void *data,
                            size_t n);
  /** @brief See manual/mulib-shylock-core.md for this operation's contract. */
  int mulib_shylock_text (mulib_shylock_buffer_t *b, const char *s);
  /** @brief See manual/mulib-shylock-core.md for this operation's contract. */
  int mulib_shylock_json_string (mulib_shylock_buffer_t *b, const char *s,
                                 size_t n);
  /** @brief See manual/mulib-shylock-core.md for this operation's contract. */
  void mulib_shylock_buffer_free (mulib_shylock_buffer_t *b);
  /** @brief See manual/mulib-shylock-core.md for this operation's contract. */
  char *mulib_shylock_dup (const char *s, size_t n);
  /** @brief See manual/mulib-shylock-core.md for this operation's contract. */
  int mulib_shylock_copy (char *dst, size_t cap, const char *s);
  /** @brief See manual/mulib-shylock-core.md for this operation's contract. */
  uint64_t mulib_shylock_hash (const void *data, size_t len);
  /** @brief See manual/mulib-shylock-core.md for this operation's contract. */
  int mulib_shylock_equal_ci (const char *a, const char *b);
  /** @brief See manual/mulib-shylock-core.md for this operation's contract. */
  int mulib_shylock_contains_ci (const char *s, const char *needle);
  /** @brief See manual/mulib-shylock-core.md for this operation's contract. */
  int mulib_shylock_contains_span_ci (const char *s, size_t n,
                                      const char *needle);
  /** @brief See manual/mulib-shylock-core.md for this operation's contract. */
  int mulib_shylock_parse_decimal (const char *s, double *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_SHYLOCK_CORE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-shylock-core.h`](../include/mulib-shylock-core.h) |
| Implementation switch | `MULIB_IMPL_SHYLOCK_CORE` |
| Repository-local includes | [mulib-utf8](mulib-utf8.md) |
| Complete example | [`examples/modules/shylock-core.c`](../examples/modules/shylock-core.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/shylock-core.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
