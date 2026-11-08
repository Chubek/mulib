# MuLIB manpage

[Manuals](README.md) · [Library index](index.md)

An owned Linux roff manpage authoring object and section emitter for plain-text
title, section, synopsis, and description fields.

## Contract and behavior

`linux()` allocates an empty document; the name selects the small
roff-output model rather than loading installed system manuals. `set()` copies
title, section, synopsis, and description, returning 1 on success and preserving
previous fields on allocation failure. `render()` requires those fields and
returns malloc-owned text or NULL; free rendered text independently of the
document. `destroy()` frees the document and accepts NULL.

Output contains TH, NAME, SYNOPSIS, and DESCRIPTION directives. Backslashes and
line breaks in supplied text are replaced with spaces, and leading roff control
characters in body fields are guarded. This is a plain-text builder, not a roff
parser or a general formatter for arbitrary manual markup.

## Example

```c
#include <assert.h>
#include <math.h>
#include <string.h>
#define MULIB_IMPL_MANPAGE
#include "mulib-manpage.h"
int main(void) {
    mulib_manpage_t *m = mulib_manpage_linux();
    assert(m);
    assert(mulib_manpage_set(m, "APP", "1", "app --help", "A tool"));
    char *t = mulib_manpage_render(m);
    assert(t);
    free(t);
    mulib_manpage_destroy(m);
    return 0;
}
```

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-manpage.h`](../include/mulib-manpage.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/**
 * @file mulib-manpage.h
 * @brief An owned Linux roff manpage authoring object and section emitter.
 * @defgroup mulib_manpage MuLIB manpage
 * @{
 * @details Produces `.TH`, `.SH NAME`, `.SH SYNOPSIS`, and `.SH DESCRIPTION`
 * roff sections. Its output is deliberately minimal; it is not a complete
 * man(7) formatter and does not accept raw roff markup. Newlines and backslashes
 * are replaced with spaces; leading roff control characters in body fields
 * are guarded.
 * @par Integration
 * In exactly one translation unit define MULIB_IMPL_MANPAGE before including
 * this file. Other translation units may include it without the implementation
 * switch.
 * @par Portability
 * ISO C11, C++17 compatible C linkage. All allocations use the C standard
 * library.
 * @}
 */
#ifndef MULIB_MANPAGE_H
#define MULIB_MANPAGE_H
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief An owned Linux roff manpage authoring object and section emitter.
   */
  typedef struct mulib_manpage
  {
    char *title, *section, *name, *synopsis, *description;
  } mulib_manpage_t;
  /** @brief mulib manpage linux; see manual/mulib-manpage.md for contracts and
   * examples. */
  mulib_manpage_t *mulib_manpage_linux (void);
  /** @brief mulib manpage set; see manual/mulib-manpage.md for contracts and
   * examples. */
  int mulib_manpage_set (mulib_manpage_t *m, const char *title,
                         const char *section, const char *synopsis,
                         const char *description);
  /** @brief mulib manpage render; see manual/mulib-manpage.md for contracts
   * and examples. */
  char *mulib_manpage_render (const mulib_manpage_t *m);
  /** @brief mulib manpage destroy; see manual/mulib-manpage.md for contracts
   * and examples. */
  void mulib_manpage_destroy (mulib_manpage_t *m);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_MANPAGE_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-manpage.h`](../include/mulib-manpage.h) |
| Implementation switch | `MULIB_IMPL_MANPAGE` |
| Repository-local includes | None |
| Complete example | [`examples/modules/manpage.c`](../examples/modules/manpage.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/manpage.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
