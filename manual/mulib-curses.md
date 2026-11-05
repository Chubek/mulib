# mulib-curses

Retained, independently owned windows over the shared vterm cell model, with
terminfo-driven differential rendering through TUI. This is a prefixed C11 API,
not an ncurses ABI replacement. Include `mulib-curses.h`; define
`MULIB_IMPL_CURSES`, `MULIB_IMPL_TUI`, `MULIB_IMPL_VTERM`, `MULIB_IMPL_GRID`,
`MULIB_IMPL_TERMINFO`, `MULIB_IMPL_TERMKEY`, `MULIB_IMPL_UTF8`,
`MULIB_IMPL_STRBUF`, and `MULIB_IMPL_TERMUTILS` in one implementation TU.
See [the runnable example](../examples/modules/curses.c).

## Ownership and drawing

`create(width,height,info)` copies the initialized terminfo object; NULL selects
the explicit ANSI profile. `newwin(width,height,x,y)` creates an independent
window with a local cursor and default style. Destroy screens and windows
separately; no borrowed window pointers are retained. All coordinates are
zero-based **x,y**. Allocation is bounded to vterm's 1,048,576 cells per buffer.
Functions return 1/0 unless their declarations specify otherwise.

`move`, `style`, `addnstr`, `erase`, `clrtoeol`, `scroll`, and `box` implement
cursor-oriented drawing. UTF-8 writes validate and stage a complete replacement
before committing. Invalid text/control scalars and non-scrolling overflow leave
the window unchanged. LF advances to column zero, CR returns to column zero,
and TAB inserts spaces to an eight-column stop. The last column uses delayed
wrap: writing the bottom-right cell succeeds; a subsequent character requires
scrolling or an explicit cursor move. `scrollok` enables automatic scrolling.
Positive explicit scroll moves contents up; negative moves down.

Like vterm/TUI, every scalar occupies one cell. Combining marks, wide-character
layout, grapheme editing and curses' global/unprefixed API are not implemented.
Styles use shared `mulib_term_style_t`; unsupported terminal rendition causes
render failure rather than silently emitting guessed sequences.

## Composition and transport

Stage windows with `noutrefresh` in paint order, then call `render` once. Staging
copies opaque cells with clipping; moving/deleting a window does not erase its
old screen area: restage the underlying window/background. `resize` preserves
screen overlap and forces repaint. Windows keep their original dimensions.

`render` atomically appends bytes and commits the front buffer; unchanged screens
produce no bytes. Deliver every appended byte before rendering again. Call
`invalidate` if delivery fails or external output changes the terminal.
`refresh(screen,fd)` handles complete writes and invalidates on transport error.
Rendering inherits TUI's 64 MiB output bound and required capabilities (`cup`,
`sgr0`, `op`, cursor visibility, and rendition-specific capabilities).

`modes(screen,enter,output)` builds optional alternate-screen/keypad entry/exit
sequences using terminfo, without performing I/O. The application owns raw mode
via `mulib_termutils_raw_begin/end`, terminal descriptors, output delivery and
cleanup on every exit path. Mode generation invalidates the display. No signal
handlers, environment lookups or process-global state are installed.

`feed` and `getkey` expose the incremental termkey decoder. Honor partial feed
counts, drain events, and pass `flush=1` at an Escape timeout or EOF. Input
supports termkey's UTF-8/VT/xterm keys, mouse and paste protocols; arbitrary
database-specific key strings are not currently decoded. Applications own
polling, resize notifications and event dispatch.
