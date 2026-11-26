# `std.llsynth`: recognize syntax with LLSynth tables

`std.llsynth` runs a LibStone LLSynth byte lexer and LR(1)/LALR(1) parser in
native On1x. It recognizes syntax, not semantic validity: it does not
convert typed tokens, build a parse tree, run grammar actions, or lower IR.
For example, a token declared `Int` may match a digit sequence outside the
i64 range. The authoritative HIR/MIR validators remain separate.

## Prepare a grammar explicitly

Use the pinned LibStone checkout (`76d24a7f8b19294bab2b713dc69f799e412b62d2`,
LLSynth `0.7.0`). With a trusted grammar, run the following from the
repository root, substituting your own grammar for the example fixture:

```sh
perl on1x/third_party/libstone/cornerstone/LLSynth/scripts/llsynth.pl \
  --mode=analyze -T python3 --driver-mode=library \
  -o parser.py on1x/stdlib/llsynth/tests/syntax.llsynth
python3 on1x/stdlib/llsynth/pack_tables.py parser.py > tables.on1x
```

The packer extracts the pinned Python emitter's `SPEC` envelope as data; it
does not import or run `parser.py`. It copies only the tables needed for
recognition and emits one ordinary On1x module. It is deliberately tied to
the pinned generator layout, not a general Python or LLSynth loader.
Run the generator only with trusted grammars, imports, tooling and host
configuration: upstream generation may load companions or invoke other
tools. The packer does not authenticate the artifact.

Put a consumer next to `tables.on1x`:

```on1x
import std.llsynth as L;
from .tables import basis_llsynth_tables;

fn main()
{
  let tables = basis_llsynth_tables();
  let (status, byte) = L.recognize(tables, b!"if hello;");
  assert(status == 0 && byte == 9);
}
```

Build it with the regular direct C11 toolchain, for example:

```sh
python3 on1x/tools/on1x-build.py consumer.on1x --on1xc /path/to/on1xc -o consumer
```

Neither importing the package nor calling the recognizer starts a process or
reads a file. Perl, Python, and LibStone are needed only when explicitly
preparing tables or running the tests, not when compiling or running a
consumer that already has a table module.
The restricted `On1xfile` source package carries the two `.on1x` modules;
use the source checkout or regular CMake install for `pack_tables.py`.

## API

`basis_llsynth_recognize(tables: buffer, source: buffer) -> (i64, i64)` is
also exported as `recognize`. Inputs are borrowed and left unchanged. Reuse a
table buffer across calls; do not mutate it while recognizing. The result is
`(status, zero_based_byte_offset)`:

| Status | Meaning | Offset |
| --- | --- | --- |
| `0` | Accepted the entire buffer | Input length |
| `1` | No lexer rule matched | Unrecognized byte |
| `2` | No parser action for the lookahead | Token start, or input length at EOF |
| `3` | Invalid image header or table data encountered during recognition | `0` for an invalid header, otherwise the current token start |
| `4` | Size or work limit exceeded | `0` for size checks, otherwise the current token start |

Lexing uses longest match, then the generator's tie order, with `^`/`$`
evaluated at LF boundaries. Input is raw bytes: NUL is permitted if the
grammar matches it, and UTF-8 is not validated separately. Tokenization is
incremental, so a syntax rejection can precede a later lexical error. The
recognizer accepts up to 512 KiB of packed tables and 1 MiB of source, and
bounds the call to 131,072 LR steps and 4,194,304 DFA row/accept inspections.
Managed stack storage is released on normal return, including rejection.
Ordinary On1x runtime limits and terminal allocation errors still apply.

The table image is a private little-endian v1 format for trusted output of
`pack_tables.py`. Offsets and references used by the parser are checked
before indexing; the reader is not an image authenticator and does not
validate unvisited rows. Do not treat acceptance as proof of table
provenance, semantic correctness, or language safety.

## Test

```sh
python3 -B on1x/stdlib/llsynth/tests/run.py /path/to/on1xc cc
```

The test generates both LR(1) and LALR(1) tables, compares the native
recognizer with upstream on representative inputs, and checks real
On1xMETA syntax and compiler-emitted MIR. It also covers malformed images,
embedded NUL, byte offsets, size and work limits, and runtime cleanup.
