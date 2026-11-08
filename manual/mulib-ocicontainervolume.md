# ocicontainervolume

[Manuals](README.md) · [Library index](index.md)

Declarative bind-volume specification. `init` normalizes absolute source and
target paths into fixed-capacity owned arrays and validates readonly=0/1. Rejects
above-root paths, controls/backslashes and root target. Returns 1 success,
0 invalid/capacity error with output unchanged. No allocation or host operations.

Lexical validation is **not symlink confinement**. A privileged mount backend
must independently enforce native path resolution, race protection, permissions,
mount options and isolation. This library does not call mount/chroot.
Switches: `MULIB_IMPL_OCICONTAINERVOLUME`, `MULIB_IMPL_VFS`.
Example: `examples/modules/ocicontainervolume.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-ocicontainervolume.h`](../include/mulib-ocicontainervolume.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-ocicontainervolume.h
 * @brief Compatibility entry point for archived OCI volume validation.
 * Define MULIB_IMPL_OCICONTAINERVOLUME and dependencies once; see its manual.
 */
#ifndef MULIB_OCICONTAINERVOLUME_COMPAT_H
#define MULIB_OCICONTAINERVOLUME_COMPAT_H
#include "mulib-vfs.h"
#include "../.deprecated/mulib-ocicontainervolume.h"
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ocicontainervolume.h`](../include/mulib-ocicontainervolume.h) |
| Implementation switch | `MULIB_IMPL_OCICONTAINERVOLUME` |
| Repository-local includes | [`.deprecated/mulib-ocicontainervolume.h`](../.deprecated/mulib-ocicontainervolume.h), [mulib-vfs](mulib-vfs.md) |
| Complete example | [`examples/modules/ocicontainervolume.c`](../examples/modules/ocicontainervolume.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ocicontainervolume.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
