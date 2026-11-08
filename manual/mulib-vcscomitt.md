# vcscomitt — compatibility spelling

[Manuals](README.md) · [Library index](index.md)

The canonical API is [`vcscommit`](mulib-vcscommit.md). This header preserves
the requested `vcscomitt` spelling without maintaining a second commit model.

Define `MULIB_IMPL_GIT`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_VCSREPO`,
`MULIB_IMPL_VCSCOMMIT`, and `MULIB_IMPL_VCSCOMITT` in one implementation unit.
Dependency implementation switches are never defined automatically.

`mulib_vcscomitt_t` and `mulib_vcscomitt_identity_t` alias the canonical types.
`mulib_vcscomitt_encode` and `mulib_vcscomitt_store` forward to the corresponding
canonical operations with identical validation, limits, ownership and return
values. Encoding produces malloc-owned bytes; storing copies into the borrowed
repository and leaves reference updates to the caller.

See [`examples/modules/vcscomitt.c`](../examples/modules/vcscomitt.c) and
`tests/test_vcs_formats.c`. Both spellings may coexist in the same program.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-vcscomitt.h`](../include/mulib-vcscomitt.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-vcscomitt.h
 * @brief Compatibility spelling for the canonical vcscommit API.
 * Define MULIB_IMPL_VCSCOMITT and all VCSCOMMIT dependencies separately.
 */
#ifndef MULIB_VCSCOMITT_H
#define MULIB_VCSCOMITT_H
#include "mulib-vcscommit.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef mulib_vcscommit_t mulib_vcscomitt_t;
  typedef mulib_vcscommit_identity_t mulib_vcscomitt_identity_t;
  /** @brief Compatibility entry points with identical ownership and failures.
   */
  int mulib_vcscomitt_encode (const mulib_vcscomitt_t *, char **, size_t *);
  int mulib_vcscomitt_store (mulib_vcsrepo_t *, const mulib_vcscomitt_t *,
                             mulib_git_oid_t *);
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
| Header | [`include/mulib-vcscomitt.h`](../include/mulib-vcscomitt.h) |
| Implementation switch | `MULIB_IMPL_VCSCOMITT` |
| Repository-local includes | [mulib-vcscommit](mulib-vcscommit.md) |
| Complete example | [`examples/modules/vcscomitt.c`](../examples/modules/vcscomitt.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/vcscomitt.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
