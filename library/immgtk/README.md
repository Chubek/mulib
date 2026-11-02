# ImmGTK: 30 optional Mune widgets

The C host in `include/mulib-immgtk.h` contains **no widgets**. These original
Lua modules use only its four drawing/input primitives. No GTK or external Lua
runtime is required. Copy this directory as `immgtk` into your module directory.

```lua
package.path = '/path/to/library/?.lua;/path/to/library/?/init.lua;' .. package.path
local ui = require('immgtk')          -- all 30 widgets
-- Or: local ui = require('immgtk.button') -- core + just this widget
```

Load outside a frame; submit drawing and interaction between the host's
`begin`/`end`. `init.lua` is the explicit all-widget loader; `core.lua` contains
only theme, validation and vertical layout helpers. Every module returns the
same per-state `ui` table without setting any global. `ui = require('immgtk')`
is convenient when submitting several separate scripts through the C host.

## Common conventions

Below, **B** means `x,y,w,h`. Unless noted, omitting B consumes the next layout
rectangle. `ui.layout(x,y,w,row=24,gap=4)` configures a vertical layout;
`ui.next()` returns its next rectangle. Layout is not reset by C frame boundaries:
call `layout` each frame. Geometry is finite and bounded as in the host; widget
boxes must have positive sizes. Text is a string. Tiny dimensions that cannot
contain a widget's decoration are rejected before interaction.

All interactive widgets take one stable positive integer `id` (<=2^53-1) and
consume **one** of the host's 256 interaction slots. No internal/generated IDs
are used, including list boxes. Submit each ID once per frame, including across
different widget types. Disabled menu items consume no interaction slot.

`clicked` means release inside a captured widget or focused keyboard activation.
Controls do not own application state: pass the returned value into the next
frame. Numeric drag widgets return `value,changed`, including changes caused by
clamping an out-of-range input. Boolean toggles return `value,clicked`.
Selection controls return `value,changed` unless their table entry says otherwise.

`normal`, `hover`, `active`, `text`, `background`, `accent` are editable ARGB theme
colors. Text uses the caller's renderer; no module measures text, changes fonts,
wraps labels or truncates strings. The application owns scissor, text layout,
window events and keyboard navigation. There is no retained widget tree.

## Widget API

| # | Module / function | Contract |
|---|---|---|
| 1 | `panel(x,y,w,h)` | Background rectangle; explicit geometry, zero sizes allowed. |
| 2 | `label(text,x,y)` | Unadorned text at an explicit position. |
| 3 | `button(id,text,B)` | Returns `clicked`. |
| 4 | `checkbox(id,text,value,B)` | Returns boolean `value,clicked`; `w>=h>=12`. |
| 5 | `slider(id,value,lo,hi,B)` | Horizontal drag, clamped to `[lo,hi]`; returns `value,changed`; sizes >=8. |
| 6 | `toggle(id,value,B)` | Sliding boolean indicator, returns `value,clicked`; `w>=2*h`, `h>=8`. |
| 7 | `radio(id,text,value,choice,B)` | Click selects non-nil `choice`, returns `value,changed`; `w>=h>=12`. |
| 8 | `selectable(id,text,selected,B)` | Highlighted row; returns boolean `selected,clicked`. |
| 9 | `progress(value,lo,hi,B)` | Noninteractive horizontal fill, clamped to range. |
| 10 | `vslider(id,value,lo,hi,B)` | Vertical drag; bottom is `lo`, top is `hi`; returns `value,changed`; sizes >=8. |
| 11 | `scrollbar(id,value,page,B)` | Vertical normalized position `[0,1]`, visible fraction `0<page<=1`; returns `value,changed`. Drag centers thumb on pointer; full page forces zero. |
| 12 | `stepper(id,value,step,lo,hi,B)` | Click left/right half to subtract/add positive `step`, clamped to range; returns `value,changed`. |
| 13 | `tab(id,text,selected,B)` | Selected underline; returns `clicked`, caller chooses active tab; `h>=4`. |
| 14 | `menu_item(id,text,enabled,B)` | Returns `clicked`; disabled items cannot capture/focus. |
| 15 | `link(id,text,B)` | Accent text with hover underline; returns `clicked`, never opens URLs; `h>=2`. |
| 16 | `badge(text,B)` | Accent background and caption. |
| 17 | `separator(B)` | Horizontal rule centered vertically. |
| 18 | `heading(text,B)` | Caption with accent underline, same caller-supplied font; `h>=2`. |
| 19 | `tooltip(text,visible,B)` | Noninteractive overlay when visible; invisible calls do not consume layout. Caller decides hover timing/position. |
| 20 | `group_box(text,B)` | Framed container, returns inset `x,y,w,h`; `w>=16,h>=32`. No implicit clipping/layout changes. |
| 21 | `collapsing_header(id,text,open,B)` | Returns boolean `open,clicked`; caller omits collapsed content. |
| 22 | `tree_node(id,text,open,depth,B)` | Indented header, returns boolean `open,clicked`; integer depth 0..64, `w>depth*12`. Caller submits children. |
| 23 | `list_box(id,items,selected,B)` | All rows visible; returns selected 1-based index and `changed`; `h>=8*#items`. |
| 24 | `combo_box(id,items,selected,B)` | **Cycling selector**, not a popup; each activation selects next item. Returns index and `changed`. |
| 25 | `image(pixels,columns,rows,B)` | Row-major ARGB pixel grid, scaled by rectangle fills; integer dimensions 1..256, exactly `columns*rows<=4096` pixels. No file decoding/filtering. |
| 26 | `color_swatch(id,color,B)` | Clickable ARGB sample with border; returns `clicked`; sizes >=4. No color picker. |
| 27 | `sparkline(values,lo,hi,B)` | Clamped sample markers, not connected/antialiased paths; 1..256 samples. |
| 28 | `histogram(values,lo,hi,B)` | Clamped bars; 1..256 samples. |
| 29 | `meter(value,lo,hi,segments,B)` | Segmented fill; 1..128 integer segments, `w>=2*segments`. |
| 30 | `spinner(phase,B)` | Eight activity blocks, caller-supplied phase in turns (wraps modulo 1). No clock access. |

All `lo,hi` ranges must be finite with `hi>lo` and a positive, finite span
representable without Mune integer overflow. Item arrays are
contiguous, nonempty lists of up to 256 strings; the supplied selection must be
a valid integer index. Pixel colors must be integers in `[0,0xffffffff]`.
Samples/values/phases must be finite numbers. Invalid arguments raise normal
Mune runtime errors. Prior drawing/Lua side effects are not rolled back.

Drag sliders and scrollbars use pointer position, not keyboard activation.
Keyboard activation of the stepper uses the pointer's left/right half; a list
box uses the pointer's row (clamped if outside). Other clickable widgets use the
host's focused activation normally. Applications needing richer keyboard or
text-entry behavior should add their own bindings/widgets, not assume a text
editor or desktop GTK implementation is present.

## Frame script

```lua
ui.panel(0, 0, 240, 240)
ui.layout(8, 8, 224, 24, 4)
if ui.button(1, 'Apply') then count = (count or 0) + 1 end
enabled = ui.checkbox(2, 'Enabled', enabled or false)
gain = ui.slider(3, gain or 0.5, 0, 1)
mode = ui.combo_box(4, {'Low', 'Medium', 'High'}, mode or 1)
ui.progress(gain, 0, 1)
```

See [`examples/modules/immgtk.c`](../../examples/modules/immgtk.c) for host setup
and explicit module loading, and [`manual/mulib-immgtk.md`](../../manual/mulib-immgtk.md)
for lifecycle, ownership, interaction and build contracts.
