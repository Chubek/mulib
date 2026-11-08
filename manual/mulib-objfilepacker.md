# objfilepacker

[Manuals](README.md) · [Library index](index.md)

`add` appends an opaque member to an initialized Unix ar byte buffer, writing
magic on first append. Names are 1..15 ASCII alnum/dot/underscore/hyphen bytes,
excluding dot/dot-dot. Metadata is deterministic (zero IDs/time, mode 100644).
Members are newline-padded to even size; size is bounded by the 10-digit ar field.

Returns 1 success, 0 invalid/overflow/OOM, failure-atomic and alias-safe. Existing
buffers are checked for magic/even length, not revalidated member-by-member.
No symbol index, GNU/BSD long names, thin archive, compression or file I/O.
Switches: `MULIB_IMPL_OBJFILEPACKER`, `MULIB_IMPL_BUFFER`.
Example: `examples/modules/objfilepacker.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-objfilepacker.h`](../include/mulib-objfilepacker.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-objfilepacker.h
 * @brief Deterministic short-name Unix ar member writer (no symbol index).
 * Define MULIB_IMPL_OBJFILEPACKER plus BUFFER. */
#ifndef MULIB_OBJFILEPACKER_H
#define MULIB_OBJFILEPACKER_H
#include "mulib-buffer.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Append a member, creating magic if buffer is empty. Names
   * are 1..15 ASCII alnum/._- bytes; no long names or special index members.
   * Content is opaque. 1 success, 0 invalid/overflow/OOM. Input may alias
   * archive storage. */
  int mulib_objfilepacker_add (mulib_buffer_t *archive, const char *name,
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
| Header | [`include/mulib-objfilepacker.h`](../include/mulib-objfilepacker.h) |
| Implementation switch | `MULIB_IMPL_OBJFILEPACKER` |
| Repository-local includes | [mulib-buffer](mulib-buffer.md) |
| Complete example | [`examples/modules/objfilepacker.c`](../examples/modules/objfilepacker.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/objfilepacker.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
