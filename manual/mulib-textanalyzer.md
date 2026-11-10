# textanalyzer — tokenization with stopwords

[Shared contracts and tests](data-search.md). Define `MULIB_IMPL_TEXTANALYZER`,
`MULIB_IMPL_TOKENIZER`, and `MULIB_IMPL_STOPWORDS`.

Init borrows source bytes and optional sorted stopword set. Next returns 1 for
a lowercase token not in that set, 0 at end or -1 for invalid/output capacity.
The output includes NUL; optional length excludes it. Capacity errors do not
consume the oversized token, though earlier skipped stopwords stay consumed.
Even a stopword must fit the buffer before it can be filtered. NULL stopword set
disables filtering. Source and vocabulary must remain alive and unchanged.
No allocation; token rules match [tokenizer](mulib-tokenizer.md).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-textanalyzer.h`](../include/mulib-textanalyzer.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/* MuLIB: Chained tokenization with optional stopword removal. C11 single-header; define
 * MULIB_IMPL_TEXTANALYZER once. */
#ifndef MULIB_TEXTANALYZER_H
#define MULIB_TEXTANALYZER_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-tokenizer.h"
#include "mulib-stopwords.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    mulib_tokenizer_t tokens;
    const mulib_stopwords_t *stopwords;
  } mulib_textanalyzer_t;
  void mulib_textanalyzer_init (mulib_textanalyzer_t *a, const void *text,
                                size_t length, const mulib_stopwords_t *stopwords);
  int mulib_textanalyzer_next (mulib_textanalyzer_t *a, char *out, size_t cap,
                               size_t *length);
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
| Header | [`include/mulib-textanalyzer.h`](../include/mulib-textanalyzer.h) |
| Implementation switch | `MULIB_IMPL_TEXTANALYZER` |
| Repository-local includes | [mulib-stopwords](mulib-stopwords.md), [mulib-tokenizer](mulib-tokenizer.md) |
| Complete example | [`examples/modules/textanalyzer.c`](../examples/modules/textanalyzer.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/textanalyzer.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
