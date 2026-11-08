# ocicontainermetadata

[Manuals](README.md) · [Library index](index.md)

`json` projects container identity/state to an owned shared JSON object with
ociVersion, id, status, exitCode. NULL means invalid/OOM; caller destroys result.
Deleted is a MuLIB extension. It deliberately does not fabricate PID or bundle
path, so this projection is **not a complete OCI state schema**. Lifecycle model
is borrowed during call; JSON copies identity. No filesystem persistence.

Switches: `MULIB_IMPL_OCICONTAINERMETADATA`, `MULIB_IMPL_OCICONTAINER`,
`MULIB_IMPL_JSON`, `MULIB_IMPL_PARSEC`, `MULIB_IMPL_STRBUF`, `MULIB_IMPL_UTF8`.
Example: `examples/modules/ocicontainermetadata.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-ocicontainermetadata.h`](../include/mulib-ocicontainermetadata.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-ocicontainermetadata.h
 * @brief Compatibility entry point for archived OCI metadata validation.
 * Define MULIB_IMPL_OCICONTAINERMETADATA and dependencies once; see its manual.
 */
#ifndef MULIB_OCICONTAINERMETADATA_COMPAT_H
#define MULIB_OCICONTAINERMETADATA_COMPAT_H
#include "mulib-json.h"
#include "mulib-ocicontainer.h"
#include "../.deprecated/mulib-ocicontainermetadata.h"
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-ocicontainermetadata.h`](../include/mulib-ocicontainermetadata.h) |
| Implementation switch | `MULIB_IMPL_OCICONTAINERMETADATA` |
| Repository-local includes | [`.deprecated/mulib-ocicontainermetadata.h`](../.deprecated/mulib-ocicontainermetadata.h), [mulib-json](mulib-json.md), [mulib-ocicontainer](mulib-ocicontainer.md) |
| Complete example | [`examples/modules/ocicontainermetadata.c`](../examples/modules/ocicontainermetadata.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ocicontainermetadata.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
