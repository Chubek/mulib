# ocicontainervfs

[Manuals](README.md) · [Library index](index.md)

`mount` routes a memory rootfs into a VFS, disabling the write operation when
readonly=1. Returns 0 success or VFS error. Root must be initialized and outlive
the mount; router borrows its backend context. Unmount/free router before root.

This is an in-memory routing policy, not chroot, native mount namespace isolation
or defense against clients that already hold the underlying mutable root object.
Switches: `MULIB_IMPL_OCICONTAINERVFS`, `MULIB_IMPL_VFS`, `MULIB_IMPL_ROOTFS`,
`MULIB_IMPL_ARCHIVE`, `MULIB_IMPL_BUFFER`.
Example: `examples/modules/ocicontainervfs.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-ocicontainervfs.h`](../include/mulib-ocicontainervfs.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-ocicontainervfs.h
 * @brief Compatibility entry point for archived OCI VFS policy adapters.
 * Define MULIB_IMPL_OCICONTAINERVFS and dependencies once; see its manual.
 */
#ifndef MULIB_OCICONTAINERVFS_COMPAT_H
#define MULIB_OCICONTAINERVFS_COMPAT_H
#include "mulib-rootfs.h"
#include "../.deprecated/mulib-ocicontainervfs.h"
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ocicontainervfs.h`](../include/mulib-ocicontainervfs.h) |
| Implementation switch | `MULIB_IMPL_OCICONTAINERVFS` |
| Repository-local includes | [`.deprecated/mulib-ocicontainervfs.h`](../.deprecated/mulib-ocicontainervfs.h), [mulib-rootfs](mulib-rootfs.md) |
| Complete example | [`examples/modules/ocicontainervfs.c`](../examples/modules/ocicontainervfs.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ocicontainervfs.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
