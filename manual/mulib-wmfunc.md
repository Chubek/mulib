# mulib-wmfunc

[Manuals](README.md) · [Library index](index.md)

Portable window-manager geometry and action helpers. Define `MULIB_IMPL_WMFUNC`,
`MULIB_IMPL_WLROOTS` and `MULIB_IMPL_VEC` in one translation unit. The geometry
model is shared with `mulib-wlroots.h`. No X11/Wayland libraries are required.

`mulib_wmfunc_init` initializes caller-owned window state with nonzero identity
and a nonempty valid half-open box. `mulib_wmfunc_fit` shrinks and clamps a box
inside a work area, using wide intermediates to avoid coordinate overflow.
Inputs may alias output. Invalid or empty inputs leave output unchanged.

`mulib_wmfunc_apply` supports place, maximize, restore, focus and close. Place
takes a box and clears maximized state. Maximize takes a work area and saves
normal geometry exactly once, so repeated maximize preserves restore placement.
Restore while normal succeeds without backend dispatch. Focus/close dispatch
requests and do not destroy the window or change geometry.

A required application callback receives the candidate state. Returning exactly
1 commits it; any other return leaves state unchanged. The callback must not
reenter or modify that window. No callbacks or pointers are retained. The
application supplies actual X11/Wayland/native backend behavior and owns native
resources; this module itself opens no displays and sends no protocol messages.
Calls are synchronous; callers synchronize each window. NULL inputs, invalid
actions, overflowed boxes and failed backend requests return 0.

See `examples/modules/wmfunc.c` and `tests/test_new_modules.c`.

`mulib_wmfunc_version()` returns API revision 1.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-wmfunc.h`](../include/mulib-wmfunc.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-wmfunc.h
 * @brief Portable window placement and transactional backend action dispatch.
 * Define MULIB_IMPL_WMFUNC, WLROOTS and VEC explicitly once.
 */
#ifndef MULIB_WMFUNC_H
#define MULIB_WMFUNC_H
#include "mulib-wlroots.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Return API revision 1. */
  unsigned mulib_wmfunc_version (void);
  typedef enum mulib_wmfunc_action
  {
    MULIB_WMFUNC_PLACE,
    MULIB_WMFUNC_MAXIMIZE,
    MULIB_WMFUNC_RESTORE,
    MULIB_WMFUNC_FOCUS,
    MULIB_WMFUNC_CLOSE
  } mulib_wmfunc_action_t;
  /** @brief Application-owned window state; id is nonzero, boxes are nonempty.
   */
  typedef struct mulib_wmfunc_window
  {
    uint32_t id;
    mulib_wlroots_box_t box, restore;
    int maximized;
  } mulib_wmfunc_window_t;
  /** @brief Backend borrows candidate state during callback. Return 1 on
   * accepted action, 0 on failure. Do not reenter/mutate this window. No
   * callback is retained.
   */
  typedef int (*mulib_wmfunc_backend_fn) (
      void *user, mulib_wmfunc_action_t action,
      const mulib_wmfunc_window_t *candidate);
  /** @brief Initialize window; output unchanged on invalid id/box. */
  int mulib_wmfunc_init (mulib_wmfunc_window_t *window, uint32_t id,
                         const mulib_wlroots_box_t *box);
  /** @brief Fit a window into a work area, shrinking if necessary. Allows
   * aliasing; output unchanged on invalid/empty boxes. Uses half-open
   * coordinates.
   */
  int mulib_wmfunc_fit (const mulib_wlroots_box_t *box,
                        const mulib_wlroots_box_t *area,
                        mulib_wlroots_box_t *out);
  /** @brief Validate and dispatch; commit state only when backend returns 1.
   * PLACE takes geometry; MAXIMIZE takes work area; others ignore geometry.
   * Repeated maximize preserves restore geometry. Restore when normal is a
   * no-op. Focus/close only dispatch requests; no native handles are owned or
   * destroyed.
   */
  int mulib_wmfunc_apply (mulib_wmfunc_window_t *window,
                          mulib_wmfunc_action_t action,
                          const mulib_wlroots_box_t *geometry,
                          mulib_wmfunc_backend_fn backend, void *user);
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
| Header | [`include/mulib-wmfunc.h`](../include/mulib-wmfunc.h) |
| Implementation switch | `MULIB_IMPL_WMFUNC` |
| Repository-local includes | [mulib-wlroots](mulib-wlroots.md) |
| Complete example | [`examples/modules/wmfunc.c`](../examples/modules/wmfunc.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/wmfunc.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
