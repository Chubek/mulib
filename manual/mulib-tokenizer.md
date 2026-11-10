# tokenizer — byte-oriented word splitting

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_TOKENIZER`.

Init borrows an explicit byte range; NULL initializes empty input. Words consist
of ASCII letters/digits and bytes ≥128; underscores, punctuation and embedded
NUL delimit tokens. ASCII uppercase is lowered; high bytes pass through without
UTF-8 validation or Unicode case folding. Next copies a NUL-terminated token
into caller storage, returning 1, 0 at end or -1 for invalid/capacity failure.
Optional token length excludes NUL. Insufficient capacity leaves cursor/output
unchanged for retry. Do not overlap output with the source or edit cursor fields.
Total iteration is O(bytes), allocation-free.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-tokenizer.h`](../include/mulib-tokenizer.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Length-aware byte tokenizer with ASCII lowercasing into caller buffers.
 * C11 single-header; define MULIB_IMPL_TOKENIZER once. */
#ifndef MULIB_TOKENIZER_H
#define MULIB_TOKENIZER_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    const unsigned char *text;
    size_t length, pos;
  } mulib_tokenizer_t;
  void mulib_tokenizer_init (mulib_tokenizer_t *t, const void *text, size_t length);
  /* Returns 1 token, 0 end, -1 invalid. Buffer receives lowercase ASCII with NUL. */
  int mulib_tokenizer_next (mulib_tokenizer_t *t, char *out, size_t capacity,
                            size_t *token_length);
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
| Header | [`include/mulib-tokenizer.h`](../include/mulib-tokenizer.h) |
| Implementation switch | `MULIB_IMPL_TOKENIZER` |
| Repository-local includes | None |
| Complete example | [`examples/modules/tokenizer.c`](../examples/modules/tokenizer.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/tokenizer.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
