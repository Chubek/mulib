# Core umbrella

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_MULIB` for `mulib_version()`. The header exposes buffer/strbuf
declarations and compile-time version constants, not every optional module.
Define `BUFFER`/`STRBUF` implementations separately if using their functions.
[Example](../examples/modules/mulib.c), [contracts](storage-server.md).

## Version and inclusion contract

`mulib_version()` returns the static string `MULIB_VERSION_STRING`; do not free
or modify it. Compile-time major, minor, patch, and string macros describe this
header's version. There is no allocation, initialization, or failure status.

The umbrella includes [buffer](mulib-buffer.md) and [strbuf](mulib-strbuf.md)
declarations. Their implementation switches are needed only when emitting their
functions, not merely to query the version. Include other libraries explicitly;
this header does not aggregate every optional API or emit shared implementations.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-mulib.h`](../include/mulib-mulib.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-mulib.h
 * @brief Small core umbrella and library version; optional modules stay opt-in.
 * Define MULIB_IMPL_MULIB for version(), and BUFFER/STRBUF switches if used.
 * Does not enable any dependency implementation implicitly. */
#ifndef MULIB_MULIB_H
#define MULIB_MULIB_H
#include "mulib-buffer.h"
#include "mulib-strbuf.h"
#define MULIB_VERSION_MAJOR 0
#define MULIB_VERSION_MINOR 1
#define MULIB_VERSION_PATCH 0
#define MULIB_VERSION_STRING "0.1.0"
#ifdef __cplusplus
extern "C"
{
#endif
  const char *mulib_version (void);
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
| Header | [`include/mulib-mulib.h`](../include/mulib-mulib.h) |
| Implementation switch | `MULIB_IMPL_MULIB` |
| Repository-local includes | [mulib-buffer](mulib-buffer.md), [mulib-strbuf](mulib-strbuf.md) |
| Complete example | [`examples/modules/mulib.c`](../examples/modules/mulib.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/mulib.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
