# On1x `std.tty` terminal profile

`tty.on1x` is a bounded terminal facade over MuLIB's `termutils`, `termkey`,
`terminfo`, `termstyle`, and `vterm` kernels. It is a **trusted raw adapter**,
not a reviewed safe FFI import. The build helper detects its externs, resolves
the dependency closure, and puts all `MULIB_IMPL_*` switches in the canonical
single implementation translation unit. `runtime.c` defines none.

The kernel-level protocol details remain authoritative:
[`termutils`](../../../manual/mulib-termutils.md),
[`termkey`](../../../manual/mulib-termkey.md),
[`terminfo`](../../../manual/mulib-terminfo.md),
[`termstyle`](../../../manual/mulib-termstyle.md), and
[`vterm`](../../../manual/mulib-vterm.md).

```on1x
import std.tty.tty as T;

fn main() {
  let (status,text)=T.basis_tty_render(1,"hello",1,2,-1,false);
  assert(status==0 || status==1); # styled or sanitized plain fallback
  println(text);
}
```

See [`examples/tty.on1x`](../../examples/tty.on1x) for event and vterm use.

## Status and lifetime contract

Every facade operation either returns status in its tuple or returns status
directly. The underlying status is process-local and single-threaded; raw extern
users must call `on1x_tty_status()` immediately.

| Status | Meaning |
|---:|---|
| `0` | Success |
| `1` | Successful sanitized plain fallback; no style/control escape emitted |
| `2` | End of file from `basis_tty_read_hex` |
| `3` | Retry/incomplete event or nonblocking read |
| `-1` | Invalid argument, malformed hex, stale/forged handle, or coordinate |
| `-2` | Native terminal/read/write I/O failure |
| `-3` | Unsupported platform or requested capability |
| `-4` | Allocation or cleanup-registration failure |
| `-5` | Input, snapshot pool, resource-slot, or screen-size limit |
| `-6` | Invalid resource state, including a second raw owner for one fd |

Raw-mode, decoder, and vterm handles are positive generation- and kind-tagged
`i64` values. Double close, stale use, forged values, and cross-resource use are
rejected. Limits are 16 raw terminals, 32 decoders, and 16 vterms. Handles are
still explicit owners because current On1x cannot express foreign-resource drop.
Close them on every path.

Dynamic strings are immutable snapshots valid until process exit. Each is at
most 2 MiB; all snapshots together, including allocation overhead, are limited
to 16 MiB. The adapter frees them at normal exit. This bounded pool is necessary
because current On1x strings use a borrowed NUL-terminated C ABI.
C-only fault-injection builds may override `ON1X_TTY_MALLOC`,
`ON1X_TTY_FREE`, and `ON1X_TTY_POOL_BYTES` consistently.

## Capability and escape policy

`basis_tty_capabilities(fd)` returns `(status,bits)`:

| Bit | Meaning |
|---:|---|
| `1` | `fd` is a terminal |
| `2` | a terminfo or explicit known-terminal ANSI control profile is active |
| `4` | indexed color is active and `NO_COLOR` is absent |
| `8` | the explicit ANSI profile plus `COLORTERM=truecolor` or `24bit` permits RGB |
| `16` | a compiled terminfo core record was loaded |
| `32` | the bounded known-terminal ANSI fallback was selected |
| `64` | `NO_COLOR` is present (its value, including empty, is irrelevant) |

The adapter never infers ANSI from `isatty` alone. It validates `TERM`, tries
`TERMINFO`, `$HOME/.terminfo`, up to 32 `TERMINFO_DIRS` entries, and fixed system
directories. If no compiled record exists, only known VT/ANSI terminal families
receive the explicit MuLIB ANSI profile. Missing/invalid/`dumb` `TERM`, unknown
terminals, non-TTY descriptors, and unsupported styling produce sanitized plain
text. Terminfo files selected through the process environment are trusted
terminal configuration; untrusted application text is not.

`NO_COLOR` disables every style escape, including attributes, but does not
disable non-style terminal operations such as cursor movement. Reset is
suppressed under `NO_COLOR`. `basis_tty_control` supports only the fixed
operation set documented in `tty.on1x`; callers cannot inject a capability name.
A missing requested control returns `-3` rather than guessing an ANSI sequence.

## Safe text and styling

`basis_tty_render` returns a capability-aware string.
`basis_tty_write` performs the same rendering and a complete `termutils` write.
Its count is the number of rendered bytes written; an I/O status can accompany
a partial count.
Input is limited to 1 MiB and must be valid UTF-8. LF and TAB pass through;
other C0 controls, DEL, and C1 controls become U+FFFD, so application text
cannot inject an escape sequence.

Style attributes use MuLIB's shared bits: bold `1`, dim `2`, italic `4`,
underline `8`, blink `16`, inverse `32`, conceal `64`, and strike `128`.
`basis_tty_attributes` builds this mask from booleans, and
`basis_tty_has_capability` checks one documented capability bit.
Colors are `-1` for terminal default, `0..255` for indexed color, or the positive
`0x01RRGGBB` value made by `basis_tty_rgb` (`-2` for invalid components). RGB is emitted directly only when
both requested and advertised; otherwise `termstyle` quantizes it to the
available indexed palette. Missing style capabilities degrade transactionally
to plain text.

Control operation numbers are reset `0`, clear `1`, zero-based cursor position
`2`, cursor hide/show `3/4`, alternate screen enter/leave `5/6`, keypad
enter/leave `7/8`, bracketed paste enter/leave `9/10`, and erase line `11`.
Convenience wrappers cover clear, cursor, visibility, alternate screen, and
bracketed paste. Returned control strings are deliberate terminal actions; send
them only to the descriptor whose capabilities were queried.

## Modes, dimensions, reads, and events

`basis_tty_raw_begin(fd)` saves POSIX attributes and enters raw 8-bit mode.
The handle borrows the descriptor, which must remain open and attached to the
same terminal. `basis_tty_raw_end` retries kernel-supported interruptions,
restores attributes, and retains the live handle if restoration fails so the
caller can retry. POSIX descriptor identity checks reject a second owner through
either the same fd or a duplicate referring to the same terminal. A normal-exit
hook restores all remaining raw handles before
freeing bridge storage. It cannot guarantee restoration after `SIGKILL`, power
loss, or arbitrary signal-handler termination; applications needing signal
policy must arrange orderly cleanup outside an async-signal handler.

`basis_tty_size` returns `(status,columns,rows)` without guessed defaults.
`basis_tty_read_hex(fd,limit)` reads 1..4096 bytes according to the descriptor's
blocking mode and returns lowercase hex. Hex is the lossless byte transport for
embedded NUL and invalid UTF-8. Use a limit of at most 128 when feeding one
fresh decoder without first pulling events.

A decoder has the MuLIB kernel's 128-byte pending bound. Feed either NUL-free
text or lowercase/uppercase hex; feed is all-or-nothing, so pull events before
retrying a full decoder. `basis_tty_decoder_next(handle,flush)` returns:

```text
(status, available, type, key, codepoint, modifiers,
 mouse_button, mouse_action, x, y, raw_hex)
```

Type values are key `0`, mouse `1`, unknown `2`, paste-begin `3`, paste-text
`4`, and paste-end `5`. Key, modifier, and mouse values are the documented
`mulib-termkey` enums. Pure helpers expose event/key/action names and modifier
membership without another native call. Coordinates are zero-based. Paste payload is raw bytes;
`raw_hex` never pretends it is UTF-8. Use `flush=false` normally and
`flush=true` only after the caller's Escape timeout or at end-of-stream.

## Virtual terminal scope

Vterms contain at most 1,048,576 cells. `basis_tty_vterm_feed` accepts NUL-free
text; `basis_tty_vterm_feed_hex` accepts up to 1 MiB of arbitrary bytes and
validates the complete hex input before mutating the screen. Cell results are
`(status,codepoint,foreground,background,attributes)`. Info returns width,
height, cursor, malformed-input count, ignored-sequence count, and visibility.

This exposes MuLIB's genuine bounded emulator: one Unicode scalar always takes
one cell. It does not claim grapheme/wcwidth behavior, a PTY, terminal replies,
scrollback, a complete Kitty protocol, or full DEC conformance.

## Verification

`on1x.tty` builds and runs an actual On1x client and example, verifies the
single-TU link plan, exercises non-TTY and `NO_COLOR` behavior, pseudo-terminal
raw/size/normal-exit restoration, binary reads, fragmented event semantics,
paste NUL bytes, malformed/bounded input, stale handles, vterm cells, and
adapter allocation exhaustion. The C boundary is compiled with strict warning
flags. No Windows behavior, concurrent calls, or async-signal cleanup is
claimed.
