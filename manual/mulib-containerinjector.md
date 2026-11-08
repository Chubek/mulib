# containerinjector

[Manuals](README.md) · [Library index](index.md)

`put` copies/replaces file content in an initialized memory rootfs, requiring
existing parent directories. Paths use VFS lexical absolute normalization;
configured node/per-file limits apply. Returns 0 success, VFS error otherwise.
Input may alias existing file storage because VFS copies before replacement.

This is data insertion into memory storage, **not process/code injection**, native
filesystem extraction, host access or permission bypass. Root owns copied bytes.
Switches: `MULIB_IMPL_CONTAINERINJECTOR`, `MULIB_IMPL_VFS`, `MULIB_IMPL_ROOTFS`,
`MULIB_IMPL_ARCHIVE`, `MULIB_IMPL_BUFFER`.
Example: `examples/modules/containerinjector.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-containerinjector.h`](../include/mulib-containerinjector.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-containerinjector.h
 * @brief Bounded file insertion into the memory rootfs, not process-code
 * injection. Define MULIB_IMPL_CONTAINERINJECTOR plus VFS. */
#ifndef MULIB_CONTAINERINJECTOR_H
#define MULIB_CONTAINERINJECTOR_H
#include "mulib-rootfs.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Copy/replace file bytes, requiring existing parent. 0 success, VFS
   * error. Only lexical in-memory paths are used, never native host paths. */
  int mulib_containerinjector_put (mulib_rootfs_t *root, const char *path,
                                   const void *data, size_t size);
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
| Header | [`include/mulib-containerinjector.h`](../include/mulib-containerinjector.h) |
| Implementation switch | `MULIB_IMPL_CONTAINERINJECTOR` |
| Repository-local includes | [mulib-rootfs](mulib-rootfs.md) |
| Complete example | [`examples/modules/containerinjector.c`](../examples/modules/containerinjector.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/containerinjector.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
