# URL rewriting

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_REWRITE_URL` and `SECURITY`. Replaces a strict origin-path
component prefix, preserving raw query bytes. No regex or redirect construction.
Returns 1 with malloc-owned output, 0 no match, -1 invalid/OOM. Output is
unchanged unless successful. [Example](../examples/modules/rewrite_url.c),
[contracts](storage-server.md#http-and-policies).

## Matching and output ownership

Prefix and replacement must be strict absolute paths, without a trailing slash
except for `/`. Matching is at a path-component boundary: `/api` matches
`/api/items` but not `/apiculture`. The target's raw query is preserved and is
not decoded or rewritten; fragments are rejected. Path validation uses the
[security helpers](mulib-seurity.md). Matching uses original path bytes rather
than percent-decoded equivalence.

Return is 1 for a rewrite, 0 for no prefix match, or -1 for invalid input,
overflow, or allocation failure. Only success updates `*out`, with newly
malloc-owned NUL-terminated text; release it with `free()`. An existing pointer
in `*out` is not freed automatically. Inputs are borrowed only for the call.
No redirect response or regular-expression substitution is generated.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-rewrite_url.h`](../include/mulib-rewrite_url.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-rewrite_url.h
 * @brief Component-prefix origin-path rewrite preserving a raw query.
 * Define MULIB_IMPL_REWRITE_URL and SECURITY. No regex or redirect generation.
 */
#ifndef MULIB_REWRITE_URL_H
#define MULIB_REWRITE_URL_H
#include "mulib-seurity.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** 1 rewritten (malloc-owned output), 0 no match, -1 invalid/OOM.
   * Prefix and replacement are strict absolute paths without trailing slash,
   * except root. Output unchanged unless 1. */
  int mulib_rewrite_url (const char *target, const char *prefix,
                         const char *replacement, char **out);
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
| Header | [`include/mulib-rewrite_url.h`](../include/mulib-rewrite_url.h) |
| Implementation switch | `MULIB_IMPL_REWRITE_URL` |
| Repository-local includes | [mulib-seurity](mulib-seurity.md) |
| Complete example | [`examples/modules/rewrite_url.c`](../examples/modules/rewrite_url.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/rewrite_url.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
