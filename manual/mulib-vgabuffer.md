# mulib-vgabuffer

Allocation-free rendering into PC VGA **text-mode** memory, from boot messages
to multiwindow TUIs. Define `MULIB_IMPL_VGABUFFER` in exactly one translation
unit. No other implementation switches, OS services, or MuLIB dependencies are
required. The header supports C11, C++ declarations/implementation, and
freestanding builds. [Runnable example](../examples/modules/vgabuffer.c).

## Storage and hardware contract

`mulib_vgabuffer_t` borrows a `volatile uint16_t` cell array, its capacity,
visible width/height and row stride, **all in cells, not bytes**. A cell's low
byte is the glyph index and its high byte is the attribute. `cell(glyph, attr)`
and `attribute(foreground, background)` construct these values. Color indices
0–15 have named constants; the attribute helper masks both arguments to four
bits. With VGA blink enabled, attribute bit 7 selects blinking, leaving only
eight background colors. With blink disabled it selects background intensity.
The application configures that hardware behavior and the palette.

The usual color aperture is physical `0xb8000`, monochrome `0xb0000`;
80 columns by 25 rows is a common configuration. Constants describe these
defaults but **do not map memory or switch modes**. Pass a suitably aligned,
already mapped pointer and the actual mode dimensions. On PC hardware cell
words are little-endian. RAM buffers use the host's numeric `uint16_t` layout.
VGA graphics modes, pixel framebuffers, font loading, palette programming,
Unicode conversion, ANSI escape parsing, and keyboard input are outside this
text-cell API. The caller supplies the active font; built-in line art assumes
CP437. Use `mulib-framebuffer.h` for its separate linear pixel surface API.

```c
#define MULIB_IMPL_VGABUFFER
#include "mulib-vgabuffer.h"

int main(void) {
    uint16_t ram[80 * 25];
    mulib_vgabuffer_t screen;
    mulib_vgabuffer_console_t console;
    /* In a kernel, ram can instead be the mapped color text aperture. */
    if (mulib_vgabuffer_init(&screen, ram, 80 * 25, 80, 25, 80) != 0)
        return 1;
    if (mulib_vgabuffer_clear(&screen, mulib_vgabuffer_cell(' ', 0x07)) != 0 ||
        mulib_vgabuffer_console_init(&console, &screen, 0x07) != 0)
        return 1;
    return mulib_vgabuffer_console_write(&console, "Hello, VGA!\n", 12) != 0;
}
```

`init` validates without clearing memory. Surfaces must be nonempty, with stride
at least width. The final visible cell must fit in capacity; final-row padding
need not be supplied. Capacities and address spans are checked for arithmetic
overflow. The caller guarantees that the declared storage actually exists and
outlives every surface, view and console. No API allocates, frees or takes
ownership. Zero-initialized surfaces are invalid. Reinitialization simply
replaces the borrowed descriptor. Keep descriptor fields unchanged after init.

All drawing skips row padding and performs volatile cell accesses. Reading is
required for `get`, `restyle`, source blits and scrolling. The application owns
display synchronization, cache policy, concurrent access and any necessary
fences. A full blit is not an atomic page flip or a guarantee against tearing.

## Drawing and windows

Functions returning `int` use **0 for success, -1 for invalid input**, except
`valid`, which is a 1/0 predicate. Validation failures do not change cells or
output descriptors. Coordinates are zero-based and unsigned. Rectangles are
`{x, y, width, height}`. Drawing clips right/bottom extents using subtraction,
including `SIZE_MAX` extents. Empty or entirely offscreen drawing is a successful
no-op. Single-cell `get`/`put` instead reject out-of-bounds coordinates.

- `clear`, `fill`, `hline`, `vline`: write complete raw glyph/attribute cells.
- `restyle`: change attributes while retaining existing glyphs, useful for
  focused controls, menu selections and disabled items.
- `box`: outline a rectangle using `border_ascii`, `border_single`,
  `border_double`, or a caller-supplied six-glyph border. Both dimensions must
  be at least two. It neither clears the interior nor invents a right/bottom
  border where an original edge lies outside the surface.
- `view`: obtain a borrowed, nonempty, fully contained subwindow with the
  parent's stride. Coordinates in subsequent operations are local to the
  window. Nested views and `view(&window, &window, rect)` work. Shared storage
  means all changes are immediately visible in the parent.
- `blit`: copy a source rectangle to a destination origin. Both bounds clip the
  copied dimensions. Attributes are preserved. Source and destination may use
  different strides. Copies within surfaces sharing the same base and stride
  are overlap-safe in all directions. Other overlapping **declared capacity
  spans**, including offset views into one allocation, return -1 before copying.
  For in-place moves between subwindows, use parent-relative coordinates on the
  common parent surface. For independent windows, use separate RAM storage.
- `scroll`: move a clipped rectangle by an unsigned number of rows, up with
  `down=0`, down with `down=1`. Fill newly exposed cells with the supplied raw
  cell. Zero rows does nothing; rows at least the clipped height clear it.

Compose a TUI by painting background/panels, drawing text and borders, restyling
the focused row, and blitting separate RAM windows in back-to-front order.
To double-buffer, build the frame in RAM, then blit its full rectangle to the
mapped display. There is no hidden front-buffer cache or event loop. Applications
can redraw only damaged rectangles using the same blit operation.

## Text and console output

`text(surface, rect, bytes, length, attribute)` draws length-delimited bytes,
including embedded NUL glyphs, into the **clipped** rectangle. One byte occupies
one cell; UTF-8 bytes are not decoded. LF begins the next row at column zero, CR
returns to column zero on the current row, and TAB writes spaces up to an
eight-column stop relative to the text area's left edge. All other bytes are
literal font indices. Text wraps to the next row but never scrolls: excess bytes
are discarded when the bottom is reached. Text exactly filling a row wraps only
before the next glyph, so a subsequent LF does not skip an extra row. The text
pointer may be NULL only for zero length and must not overlap destination cells.
Text does not clear unwritten cells; fill the area first when replacing labels.

`console_init` borrows a surface or subwindow without clearing it, sets the cursor
to `(0,0)`, and selects tab width `min(8, width)`. `console_write` retains cursor
and delayed-wrap state across calls. It interprets LF, CR and TAB as above, with
configurable tab width, and BS moves left without erasing or crossing a row
boundary. BS at pending wrap keeps the cursor in the final column and cancels
the wrap. Other bytes, including NUL and ESC, are literal glyphs. At the bottom,
newlines and wrapped output scroll the window up and fill with spaces in the
current attribute. A write that fills the final cell leaves it visible until
more output actually requires scrolling, even on a one-cell surface.

Applications may set `console.attribute` and `console.tab_width` (1 through
surface width), inspect `x`, `y` and `wrap_pending`, and use `console_move` to
reposition and cancel pending wrap. Other state is maintained by the API.
Console input must not alias destination storage. A zero-length write leaves
state unchanged. Software cursor changes do not access hardware automatically.

## Hardware cursor

`cursor(io, crtc, position, start, end, visible)` uses supplied byte port-I/O
callbacks; each returns 0 or -1 and receives `io.context`. An adapter may call
`mulib_io_in8`/`mulib_io_out8` (with their own implementation switch and platform
configuration) or kernel-specific functions. Mock callbacks suffice for tests.

Select `MULIB_VGABUFFER_COLOR_CRTC` (`0x3d4`) or
`MULIB_VGABUFFER_MONO_CRTC` (`0x3b4`). `position` is the absolute cell offset in
text memory, from 0 through 65535, **not a byte address**. Include the display
page's start offset and a view's origin: for a full-screen console it is usually
`page_start + console.y * stride + console.x`. The caller must check this sum
before passing it. Cursor start/end scanlines must satisfy
`0 <= start <= end <= 31` and fit the actual font to display as intended.
`visible` is exactly 0 or 1.

The function reads CRTC registers 0x0a/0x0b, writes location 0x0e/0x0f, then
updates shape/visibility while preserving unrelated high bits. Invalid arguments
make no I/O calls. A callback failure stops immediately and returns -1; hardware
may already be partly updated. Serialize the entire transaction with other CRTC
users. The function leaves the index register at the last accessed register.
Display start-address programming remains the caller's responsibility.

## Verification

`tests/test_vgabuffer.c` exercises guard cells and row padding, extreme extents,
subwindow ownership, exact CP437 borders, embedded bytes, clipped text, every
small overlapping-copy direction against a snapshot oracle, scroll boundaries,
single-cell consoles, split-stream equivalence, and cursor callback failure at
every I/O step. It operates on RAM and mocked registers; physical VGA hardware
is not required by the example or test.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude tests/test_vgabuffer.c -o /tmp/opencode/test-vgabuffer
/tmp/opencode/test-vgabuffer
cc -std=c11 -Iinclude examples/modules/vgabuffer.c -o /tmp/opencode/vgabuffer
/tmp/opencode/vgabuffer
```
