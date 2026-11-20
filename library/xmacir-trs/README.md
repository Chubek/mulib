# XMacIR m4 rewrite passes

These opt-in passes take one validated [XMacIR](../../manual/mulib-xmacir.md)
program and emit one program with the same entry and ABI. They are
standalone GNU m4 files: no shell commands, auxiliary files, or
generated C are needed.

| Pass | U32 | BLOB |
| --- | --- | --- |
| `peephole.xmtrs` | Remove `PUSH`/`DROP`, `DUP`/`DROP`, consecutive `SWAP`s, and right-operand identities. Fold adjacent constant pairs within the safe arithmetic bounds below. | Pass through all records. |
| `strength.xmtrs` | Replace `x * 2` with `DUP`/`ADD`. Replace `x * 0`, `x & 0`, and `x \| 4294967295` with `DROP` followed by the constant result. | Pass through all records. |
| `commute.xmtrs` | Eliminate `SWAP` before `ADD`, `MUL`, `AND`, `OR`, or `XOR`; remove swaps of duplicated values and cancel pairs of swaps. | Pass through all records. |
| `repack.xmtrs` | Pass through all records. | Pack unchanged bytes in order into 32-byte records, plus a final short record if needed. Remove empty records; an empty payload needs no byte records. |

`peephole` recognizes `+0`, `-0`, `*1`, `&4294967295`, `|0`, and `^0`.
For other pairs it only folds when **both** operands are at most `32767`.
The largest intermediate is `32767 * 32767`, which fits a signed 32-bit
integer. Subtraction below zero wraps modulo 2³², using decimal limbs to
format the result without feeding a large literal to `m4_eval`. Larger
constants remain valid input and are not folded except by the identity
rules.

`strength` does not evaluate operands in m4. The original and replacement
produce the same unsigned 32-bit result, and the replacement does not
increase peak stack depth or instruction count. `commute` does not move a
swap across noncommutative operations such as `SUB`. Each pass has bounded
look-behind; none attempts general or fixed-point optimization. Choosing
`commute` before `peephole` can expose neighboring constants for folding.

## Input and invocation

Use the XMacIR parser and canonical formatter before m4. The stream is
ASCII, one record per line, with an LF after every record. A scalar
program starts with `XM_U32(entry)` and ends with `XM_RET()`: stack
operations must be valid and leave exactly one value for return.
`XM_PUSH` literals are unsigned 32-bit decimal with no leading zero
except for `0`. A capsule starts with
`XM_BLOB(entry,bridge,context_tag,error_tag)`, has zero or more
`XM_BYTES(h...)` rows (0–64 lowercase hex digits, even length), and
ends with `XM_END()`.

Every identifier field must be validated before it is passed to m4.
Reject identifiers starting with `m4_` or `XM_`: m4 expands words on
input, including while collecting arguments. `-P` moves GNU m4 built-ins
into `m4_`; all pass-local names live in `XM_`. The passes quote emitted
records so m4 does not expand its own output. Do not pipe user text or C
source directly into these passes.

Use an argument vector equivalent to this command (for already
validated input):

```sh
m4 -P -- library/xmacir-trs/peephole.xmtrs - < validated.xmir > rewritten.xmir
```

Select one file per invocation. To chain passes, validate the output
of each pass before passing canonical input to the next. Reject
malformed output or a changed program kind, entry, or BLOB ABI;
clients carrying opaque payloads (including Termscript) must also
require identical decoded BLOB bytes, not identical chunk boundaries.
Custom rules are trusted build inputs, not sandboxed code or proven
semantic transformations. Only opt into known rule files and a
trusted m4 executable.

To test the passes directly: `python3 tests/test_xmacir_trs.py`.
