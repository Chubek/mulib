# Termscript terminal profile

Terminal capabilities are separate from Termscript words. `Terminfo.man` is
the reference for the short capability names and parameter conventions.
`vt100.txt` (CUP, ED, EL, SGR and DECSTBM) and
`ecma-48-code-sequence.txt` (notably ECMA-48 §8.3.21, §8.3.39, §8.3.41 and
§8.3.117) govern the ANSI subset. Termfunc accepts zero-based coordinates
in column, row order and supplies row, column to `cup`. No text or command
literal may be interpolated into an escape template.

The explicit xterm profile adds DECSET/DECRST (`CSI ? Ps h` and `CSI ? Ps l`)
for these modes; they are not part of the generic ECMA-48 profile:

| Parameter | Mode |
| --- | --- |
| 25 | Show cursor |
| 1000 | Button-event mouse tracking |
| 1006 | SGR mouse encoding |
| 1049 | Alternate screen and saved cursor |
| 2004 | Bracketed paste |

With SGR mouse use `1000` followed by `1006` to enable, and undo them in
reverse order. Bracketed paste input is delimited by `CSI 200 ~` and
`CSI 201 ~`. Input decoding and user-data sanitization are independent of
privileged terminal-mode and terminfo operations.
