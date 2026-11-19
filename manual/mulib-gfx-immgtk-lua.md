# ImmGTK Lua overlay safety

`require('immgtk')` and the existing flat widget modules still return the same
legacy table. Extensions are opt-in: `require('immgtk.extensions')` and grouped
modules such as `immgtk.controls.input_text` return a separate, shared advanced
table. Load modules outside the native host's rendering frame.

## Frame and scope lifetime

```lua
local a = require('immgtk.extensions')
a.frame({text = '', backspace = false, dx = 0, dy = 0, scroll = 0}, function()
  a.begin_window('main', 'Application', bounds)
  value = a.input_text('entry', value, 16, 40, 200, 24)
  a.end_window()
end)
```

`frame(input, callback)` snapshots input, runs the callback under `pcall`, and
restores ID scopes, extension-owned clips, containers and legacy layout on
success **or** failure. Callback errors retain their original Lua error object.
Callback return values, including interior/trailing `nil`, are preserved.
`with_id(key, callback)` and `with_clip(x,y,w,h,callback)` provide the same
structural cleanup for smaller scopes. The clip callback receives a boolean
indicating whether native clipping is available.

For explicit submission, call `begin_frame(input)` and balance every push/begin.
`end_frame()` unwinds forgotten extension scopes and reports an error if any were
unbalanced. `begin_frame` refuses to silently discard outstanding scopes.
Window and child ends must follow one shared LIFO order; child tokens are checked
by identity. Guards cannot pop a caller's outer scopes or containers.

Native clips require **both** `imm_clip_push` and `imm_clip_pop`. With a missing
or partial pair, `clip` returns false without calling either hook. A successful
push pins its matching pop function. Hooks may return no values on success;
explicit `false` or an error indicates failure. A failed push must not modify the
native stack; a failed pop must leave its entry in place so cleanup can retry.
The C host must still reset its clip stack after script errors, especially after
native allocation failures or VM resource-limit aborts that prevent Lua cleanup
from running. Lua cannot recover an incorrectly implemented hook.
Missing native clipping does **not** imply software text/hit clipping: partial
list rows, plots and custom content may extend outside a viewport.

## Input, IDs and bounds

`begin_frame` copies only `dx`, `dy`, `scroll`, `text`, `backspace`, and
`context_menu`; it does not retain the supplied table or expose native resources.
Deltas are finite with absolute value at most 1,000,000. Boolean inputs must be
booleans, not numeric zero/one. Text is strict UTF-8, at most 4,096 bytes per frame.
Invalid input leaves the previous snapshot unchanged.

Text entry accepts at most 65,536 bytes, validates before native interaction,
and applies the frame's backspace/text pulse at most once across focused entries.
Backspace removes one UTF-8 scalar, **not** a grapheme cluster. This append-only
editor has no cursor, selection, IME, shaping, clipboard or undo support.
Input-number buffers may temporarily be invalid numbers; the original numeric
value is preserved until the buffer parses as finite.

Extension ID keys are strings (at most 1,024 bytes), finite numbers within
`[-(2^53-1),2^53-1]`, or booleans. Types and scope lengths are encoded separately,
so numeric `1` and string `"1"` do not collide. Tables/functions/userdata are
rejected without invoking `__tostring`. IDs are stable for the loaded module's
lifetime; at most 65,536 distinct paths and 8 MiB of encoded path bytes (excluding
table overhead) are retained. Do not mutate the advanced
table's internal `ids`, `scope`, `next_id`, or input snapshot. There are at most
64 nested ID scopes, native clips, and containers. IDs start at `2^52`; do not
manually allocate IDs in the reserved extension/retained ranges.

Fallback lines validate geometry/color and emit at most 2,049 one-pixel
rectangles. They are bounded sample lines, not a complete antialiased rasterizer.
Native `imm_line` is preferred when present.

## Transaction boundaries

- Legacy images validate **all** colors and snapshot them before any drawing or
  implicit layout consumption. Mutation during a native callback cannot replace
  a later validated color. Invalid legacy slider values/ranges do not consume
  layout or interaction IDs.
- Buttons, selectables, menu items and collapsing headers reject invalid labels
  before interaction. Existing valid arguments and return conventions remain.
- Extension vectors validate every component before interaction and stage writes
  until all components succeed. Vectors are plain tables of 1–16 values.
- Range sliders validate both endpoints before either control. Color pickers
  validate their ARGB input and report changes after integer quantization.
- Window dragging stages position changes until setup succeeds. Child/window
  setup, scroll callbacks and list rendering restore structural scopes on errors.
- Virtual-list indices use half-open viewports, including empty viewports.
  Counts are integers in `[0,10^9]`; offsets are in `[0,10^12]`; height and positive
  row size are at most 1,000,000. `first > last` denotes no visible rows.
  List views accept up to 65,536 strings and at most 4,096 visible rows.

These are **not** whole-frame transactions: already drawn pixels, consumed native
interaction slots, user callback side effects, and earlier successfully completed
widget calls cannot be rolled back. Other grouped editors/layout/menu/plot helpers
remain baseline implementations; wrap custom submissions in `frame` for error
cleanup, and do not assume all grouped helpers prevalidate entire models.
Native host limits still apply, including Mune execution/heap budgets and the
legacy host's 256 interaction slots.

## Validation

From the repository root:

```sh
lua tests/test-gfx-immgtk-lua.lua
cc -std=c11 -Iinclude tests/test_immgtk.c -lm -ldl -pthread -o /tmp/opencode/test-immgtk
/tmp/opencode/test-immgtk "$PWD/library"
```

The Lua regression deliberately uses stub-native hooks and tests validation,
mutation/failure injection, frame-input ownership, UTF-8, scoped cleanup and
legacy module identity. It can also run in the repository's Mune runtime:

```sh
cc -std=c11 -Iinclude tests/test_mune_runtime.c -lm -ldl -pthread -o /tmp/opencode/mune-runtime
/tmp/opencode/mune-runtime --run tests/test-gfx-immgtk-lua.lua
/tmp/opencode/mune-runtime --vm tests/test-gfx-immgtk-lua.lua
```

`test_immgtk.c` runs the existing `tests/immgtk-widgets.lua` using actual Mune and
the original C host, including pixel and press/release expectations. Stub-native
tests do not validate the additive native Mune bridge, fonts, clipping semantics,
desktop protocols or hardware. Native text input/event queues and the advanced
bridge still require separate C integration testing.
