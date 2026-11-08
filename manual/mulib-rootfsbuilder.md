# rootfsbuilder

[Manuals](README.md) · [Library index](index.md)

`export` appends a complete USTAR archive from a memory rootfs without consuming
it. Entries follow storage insertion order. File mode 0644, directory mode 0755,
uid/gid/mtime zero; source metadata is not preserved. Paths must fit the existing
USTAR writer profile. Returns 0 success, negative invalid/archive/OOM, atomically.

Archive bytes are caller-buffer-owned. No native filesystem walking, compression,
whiteout layering, hardlinks/symlinks or image signing. See [container limits](extension-layers.md#containers-and-security-sensitive-adapters).
Switches: `MULIB_IMPL_ROOTFSBUILDER`, `MULIB_IMPL_ROOTFS`, `MULIB_IMPL_VFS`,
`MULIB_IMPL_ARCHIVE`, `MULIB_IMPL_BUFFER`. Example: `examples/modules/rootfsbuilder.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-rootfsbuilder.h`](../include/mulib-rootfsbuilder.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-rootfsbuilder.h
 * @brief Deterministic USTAR export from the bounded memory root filesystem.
 * Define MULIB_IMPL_ROOTFSBUILDER plus ROOTFS, VFS, ARCHIVE and BUFFER. */
#ifndef MULIB_ROOTFSBUILDER_H
#define MULIB_ROOTFSBUILDER_H
#include "mulib-buffer.h"
#include "mulib-rootfs.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Append complete archive atomically, insertion-order entries,
   * uid/gid/mtime=0, modes 0755 directories/0644 files. Returns 0 success,
   * negative error. Long paths outside USTAR writer profile are rejected. Does
   * not consume root. */
  int mulib_rootfsbuilder_export (const mulib_rootfs_t *root,
                                  mulib_buffer_t *out);
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
| Header | [`include/mulib-rootfsbuilder.h`](../include/mulib-rootfsbuilder.h) |
| Implementation switch | `MULIB_IMPL_ROOTFSBUILDER` |
| Repository-local includes | [mulib-archive](mulib-archive.md), [mulib-buffer](mulib-buffer.md), [mulib-rootfs](mulib-rootfs.md) |
| Complete example | [`examples/modules/rootfsbuilder.c`](../examples/modules/rootfsbuilder.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/rootfsbuilder.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
