# mulib-immgtk

A widget-free immediate-mode GUI host. `include/mulib-immgtk.h` supplies only
drawing, input and lifecycle primitives; it contains no widget source or theme,
and does not load files. The optional **30 Mune Lua widgets** are separate modules
under [`library/immgtk`](../library/immgtk/README.md). It renders into borrowed
GLX surfaces; the application supplies window/event processing and a UTF-8 text
renderer.

## Integration

Define the following once before inclusion (the runnable example lists them):

```text
MULIB_IMPL_IMMGTK MULIB_IMPL_GLX MULIB_IMPL_DRM MULIB_IMPL_LINALG
MULIB_IMPL_MUNE MULIB_IMPL_DYNLOAD MULIB_IMPL_MIR MULIB_IMPL_VMWEAVE
MULIB_IMPL_EGRAPH MULIB_IMPL_REWRITE MULIB_IMPL_DYNCALL MULIB_IMPL_JIT
MULIB_IMPL_PARSEC MULIB_IMPL_PARSER MULIB_IMPL_PEG MULIB_IMPL_AST
MULIB_IMPL_STRBUF MULIB_IMPL_GLOB
```

Build with C11, `-pthread -lm -ldl` on Linux. No external GTK/Lua library is linked.
`create(text_callback,user,error)` owns a fresh Mune state and binds only
`imm_rect`, `imm_text`, `imm_pointer`, and `imm_interact`. There is no `ui` global
until application code creates one. Callback and user storage must outlive the GUI. A text callback is
required and returns 1 on successful drawing. Its string is borrowed only for
the call and includes an explicit byte length. Callback drawing should honor
GLX scissor. NULL creation result reports a static diagnostic. Destroy releases
the state and GUI, never the surface or callback data; NULL destruction is safe.

`state()` borrows the Mune state for application globals, functions and bindings.
Keep application-owned Lua objects rooted according to Mune's ownership rules.
Contexts are externally synchronized; callbacks must not re-enter/destroy them.

### Explicit widget loading

Before drawing, use `run` to configure Mune's normal module search path and load
the external library (paths below are relative to the process working directory):

```lua
package.path = 'library/?.lua;library/?/init.lua;' .. package.path
ui = require('immgtk')
```

Use an absolute installation path when running elsewhere. `require('immgtk')`
loads all 30 widgets; `local ui = require('immgtk.button')` loads only the shared
core and button. Modules return the same state-local theme/layout table and never
create globals themselves. Loading is cached by Mune's `package.loaded`.
Applications may instead install these named modules through Mune bundles or
`package.preload`; the C header has no runtime dependency on the library files.
Missing files or malformed modules produce ordinary Mune errors from `run`, not
creation failures. Loading is not transactional: earlier modules remain loaded
if a later module fails.

**Migration:** code that previously used `ui` immediately after `create` must
explicitly load the library. Existing button/checkbox/slider/label/panel/layout
signatures are preserved. Ship `library/immgtk` alongside the application if
using file-based loading. The example accepts the parent `library` directory as
its first argument, defaulting to `library` when run from the repository root.

## Frames and widgets

1. Call `begin(gui,surface,input)` with pointer position, down state and an optional
   one-frame keyboard activation pulse.
2. Call `run` with Mune code that submits each visible interactive widget once.
3. Call `end` even if script execution fails.

Begin/end return 1 on success, 0 on invalid/nested/missing frames. A surface is
borrowed until end. `run` returns a Mune status and releases its result list;
globals persist. Definition-only code can run outside frames. Drawing/input
primitives outside a frame fail with a Mune runtime error. Runtime diagnostic
text is borrowed until the next execution. Earlier drawing/Lua side effects
remain committed on error, matching Mune's execution contract.

```lua
ui.panel(0, 0, 240, 160)
ui.layout(8, 8, 224, 24, 4)
if ui.button(1, 'Apply') then count = (count or 0) + 1 end
enabled = ui.checkbox(2, 'Enabled', enabled or false)
gain = ui.slider(3, gain or 0.5, 0, 1)
```

Functions with omitted geometry consume `ui.next()`'s next layout rectangle.
Explicit forms are `button(id,text,x,y,w,h)`,
`checkbox(id,text,value,x,y,w,h)` and `slider(id,value,min,max,x,y,w,h)`.
Button returns clicked. Checkbox returns value,clicked; slider returns value,changed.
Checkbox requires width>=height>=12. Slider requires width,height>=8 and max>min.
`label(text,x,y)` and `panel(x,y,w,h)` are noninteractive. `ui` color fields
`normal`, `hover`, `active`, `text`, `background`, `accent` are editable ARGB values.
The full widget API, bounds and simplified behavior are documented in
[`library/immgtk/README.md`](../library/immgtk/README.md). The library adds
toggles, radio/selectable controls, vertical sliders, scrollbars, steppers, tabs,
menu items, links, badges, separators, headings, tooltips, group boxes,
collapsing headers, tree nodes, list/cycling selectors, pixel images, color
swatches, sparklines, histograms, segmented meters and activity spinners.

IDs must be stable positive integers <=2^53-1; duplicate IDs or more than 256
interactive widgets in a frame cause a runtime error. A press captures the first
submitted hovered widget, dragging preserves capture, and release clicks only
if the pointer is over that widget. Missing/released capture clears at end.
Press also sets focus; activation pulses click the focused widget. Focus clears
when that widget is absent. Keyboard traversal/text entry are application work.
Hot/active/focus accessors return the ID or zero. Overlaps use first-submitted
priority. Geometry is finite with |x|,|y|,width,height<=1e6; interactive sizes
must be positive. Hit regions are the submitted rectangles, independent of GLX
scissor; applications should omit hidden/clipped interactive widgets.

Custom widgets are ordinary Mune functions using `imm_rect(x,y,w,h,argb)`,
`imm_text(text,x,y,argb)`, `imm_pointer()` (x,y), and
`imm_interact(id,x,y,w,h)` (hot,held,clicked,focused). Mune's normal bounded
runtime/parser defaults apply. No retained widget tree is created.

`examples/modules/immgtk.c` runs a headless press/release and records text requests;
replace its callback with a font renderer for a graphical application.
`tests/test_immgtk.c` and `tests/immgtk-widgets.lua` verify widget-free creation,
explicit/partial loading, drawing all 30 widgets, all 17 interactive controls,
validation, capture, release cancellation, focus activation, custom Lua widgets,
duplicate IDs, the interaction limit and frame lifecycle.
