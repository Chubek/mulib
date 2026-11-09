# Mu3AC: MuLL's three-address native backend

Mu3AC lowers **analyzed MuLLIER**, not raw MuLL text, into per-function,
non-SSA register code. Independent implementations are available through:

```cpp
#include "MuDSL.hpp"
#include "mull/mu3ac/Mu3AC.hpp"
mull::Runtime runtime;
auto ir = runtime.compile("return 6*7;", "answer.mull");
auto program = mull::mu3ac::Lowering{}.compile(ir);
mull::mu3ac::Rewriter rewrite;
rewrite.optimize(program);
auto assembly = mull::mu3ac::X86_64{}.emit(program);
```

```d
import MuDSL;
import mull.mu3ac.Mu3AC;
auto runtime = new MuLL.Runtime;
auto program = (new Lowering).compile(runtime.compile("return 6*7;", "answer.mull"));
(new Rewriter).optimize(program);
auto assembly = (new X86_64).emit(program);
```

Both hosts now expose `Runtime.parse(source,name)` and
`Runtime.compile(source,name)`. Neither evaluates user code or imports packages.
`parse` returns the original tree in the MuLLIER interchange format; `compile`
performs existing lexical/control analysis and optional equality saturation with
the runtime's known global-binding mutability. `run` keeps its interpreter
behavior and existing API.

## Representation

`Program` contains `Function`s, each with parameter names, a virtual-register
count, and an ordered instruction sequence. Each instruction carries a source
line/byte-column. `Program.dump()` emits the same version-1 S-expression format
in both languages:

```scheme
(mu3ac 1
  (function 0 2 (params)
    (tick (site 1 1))
    (number 1 "42" (site 1 8))
    (return 1 (site 1 1))
  )
)
```

That example is schematic, not the exact register numbering of every source.
The CLI emits this format for inspection; it does not yet accept it as source.

| Family | Operations and semantics |
| --- | --- |
| Values | `nil`, `number`, `string`, `literal`, `move` |
| Bindings | `load`, `bind`, `check-store`, `store`, `alias` |
| Computation | `unary`, `binary`, `member`, `index` |
| Scope | `enter`, `leave` |
| Control | `label`, `jump`, `branch-false`, `return` |
| Calls/data | adjacent `param` instructions followed by `call`, `list`, or `record`; `closure` references another function |
| Budget | `tick`, emitted for evaluated source nodes |

Arithmetic has at most two value inputs and one result. General calls use
explicit parameter instructions. Values are opaque 64-bit runtime handles;
registers are initialized to nil and may be assigned at branch joins. Scope and
binding cells are distinct from temporary registers. `and`/`or` lower to branches
and moves, preserving operand-valued results and left-to-right effects.

Closures retain live lexical environments, including later declarations and
references. Function creation does not evaluate its body. Loop breaks unwind
exactly the active body scopes; a partial iteration does not replace the last
completed iteration's result. `check-store` precedes the assignment RHS so an
invalid or immutable target fails before RHS effects.

The lowering accepts all current MuLL AST operations, including collections,
closures, references, loops, and host-backed `mulib` values. It bounds functions
at 4096, registers per function at 4096, total instructions at 262144, and literal
arguments/parameters/collection entries at 256. Oversized or unsupported input
is rejected, not executed by a hidden interpreter fallback. These bounds do not
limit the sizes of collections returned by library calls.

## Rewriting

`Rewriter` uses the host toolkit's **MuDSL e-graph matcher, bounded saturation,
and extraction**, with shared defaults in `rewrite/default.t`. It tracks local
facts about already-evaluated scalar registers. Facts are discarded at control
flow boundaries and conservatively invalidated on register reassignment.
Original value-producing instructions, error checks, and source ticks are not
discarded or reordered. Default rules include numeric unary-plus elimination,
double numeric negation, and truth negation of known scalar values. They do not
apply unsafe `x+0`, `x*0`, or floating-point reassociation identities.

```scheme
(rewrite positive-number (positive (number ?r)) (use ?r))
(rewrite not-nil (not (literal "nil")) (literal "true"))
```

The `number` fact names an existing computed register, not an unevaluated
numeric expression. Supported replacement forms are `(use REGISTER)` and
`(literal "true"|"false"|"nil")`. Custom equalities are trusted compiler
extensions, not automatically proven theorems. `loadRules` replaces a parsed
rule set transactionally and bounds input at one MiB; default saturation limits
are 8 iterations, 512 nodes, and 1024 matches per candidate instruction.
`changes` reports the number of replaced instructions.

After editing the shared rules:

```sh
python3 mudsl/mull/mu3ac/rewrite/embed.py
python3 mudsl/mull/mu3ac/rewrite/embed.py --check
```

## Native code and ABI

`X86_64.emit` produces GNU assembler text for **Linux ELF/System V AMD64**.
The backend emits real native function bodies, branch labels, scope operations,
calls, and returns; it never embeds entry source for `Runtime.run`. Baseline
code generation is deliberately simple: handle registers occupy stack slots,
call arguments occupy a bounded scratch area, and runtime helper calls operate
on the selected host's existing dynamic `Value` implementation.

The emitter preserves the platform's stack alignment and callee-saved frame
pointer, emits CFI unwind descriptions so C++/D errors can traverse generated
frames, uses length-bearing byte views for strings/record keys, and marks the
stack non-executable. It checks register ranges, function references, labels,
and parameter/consumer balance. No assembler or shell is invoked by the library.

Generated objects export `mu3ac_entry` and `mu3ac_config`, with a versioned,
136-byte native configuration record and a private `mu3ac_*` helper ABI.
`Runtime.cpp`/`Runtime.d` implement that ABI independently; `Main.cpp`/`Main.d`
provide process startup. The CLI links the matching installed runtime archive.
Do not mix the private native ABI across releases or assume native artifacts
are a portable bytecode format. Each generated object is an entry module, not
a general multi-module linking format.

Native calls still support ordinary `G`, standard-library records, package
closures, and host callbacks. Imported source packages currently run in the
existing interpreter. Backend accounting hooks share source-step, script-call,
and environment budgets across this boundary. The flat backend does not use
the interpreter's AST evaluation-depth guard; parser/IR depth, call depth, native
stack availability, and its own register/frame budgets remain relevant.
Optimization and backend allocation strategies need not consume identical
budgets. This is not a native-code performance claim or a security sandbox.

The native context retains handles/values and environments until execution
finishes. The additional `maxValues` arena budget defaults to two million;
large loops can reach it before exhausting the source-step budget. Package files,
custom MuLLIER rule directories, and shared foreign libraries are external
runtime dependencies, not embedded assets. Explicit double-only C-ABI bindings
are described in [`../cli/README.md`](../cli/README.md) and `mull(1)`.

## Verification

`mull_mu3ac` checks compile-only behavior, local rewriting, transactional rule
reloads, malformed branch/register rejection, and unwind/non-executable-stack
assembly directives. `mu3ac_embedded_rules` checks snapshot reproducibility.
`mull_cli` performs cross-host, optimized/unoptimized interpreter/native
differential checks against the existing language fixtures and exercises actual
ELF emission, third-party linking, budgets, arguments, and CLI failures.
