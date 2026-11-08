# mulib-listener

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_LISTENER` and `MULIB_IMPL_EVTSTREAM` in one translation unit.

`mulib_listener_walk(root,listeners,count)` consumes AST events and dispatches
`mulib_listener_t` entries in table order. Each entry contains a production name
(NULL means all productions), optional `enter` and `leave` callbacks, and user
context. Names match productions, not semantic-action aliases.

Callbacks return nonzero to continue or zero to stop. The walk returns 1 on
completion, 0 on an early stop, -1 for invalid arguments. Early stop does not
unwind pending leave callbacks. Trees, tables and contexts remain borrowed;
callbacks must not change the traversed tree.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-listener.h`](../include/mulib-listener.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-listener.h
 * @brief Production-specific enter/exit AST callbacks. Define
 * MULIB_IMPL_LISTENER; explicitly implement EVTSTREAM too. Return zero from a
 * callback to stop.
 */
#ifndef MULIB_LISTENER_H
#define MULIB_LISTENER_H
#include "mulib-evtstream.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_listener
  {
    const char *production; /* NULL matches every production. */
    mulib_ast_action_fn enter, leave;
    void *user;
  } mulib_listener_t;
  int mulib_listener_walk (const mulib_ast_t *root,
                           const mulib_listener_t *listeners, size_t count);
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
| Header | [`include/mulib-listener.h`](../include/mulib-listener.h) |
| Implementation switch | `MULIB_IMPL_LISTENER` |
| Repository-local includes | [mulib-evtstream](mulib-evtstream.md) |
| Complete example | [`examples/modules/listener.c`](../examples/modules/listener.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/listener.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
