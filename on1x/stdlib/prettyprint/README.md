# `std.prettyprint` — language-neutral structured pretty-printing

This On1x package constructs an immutable S-expression **document IR** and passes it across a raw scalar C ABI into the MuLIB `prettydoc`, `prettyemit`, and `prettysexpr` kernels. It does not presume that the document came from an On1x AST. Language adapters for C, C++, Rust, ML, compiler IR, JSON, etc. build the same IR.

The package is intended to live at `on1x/stdlib/prettyprint`, and import as `import std.prettyprint as Pretty;`.

## Example

```on1x
import std.prettyprint as Pretty;
fn main() {
  let args=sx_cons(Pretty.pp_text("alpha"),sx_cons(Pretty.pp_text("beta"),sx_nil()));
  let doc=Pretty.pp_call("f",args);
  let (status,source)=Pretty.pp_format(doc,12);
  assert(status==0);
  println(source);
  let (html_status,html)=Pretty.pp_render(doc,12,1,1,0);
  assert(html_status==0);
  println(html);
}
```

Use `pp_region("search",doc)` or `pp_paragraph("knuth-plass",doc)` to select an algorithm for a subtree. `pp_render(doc,width,algorithm,target,paragraph)` returns `(i64,string)` where status=0 means success. `pp_format(doc,width)` selects fit-based plain source. Algorithm 0 is local greedy, 1 group-fit, 2 bounded search; target 0 is plain text, 1 HTML/MathML, 2 LaTeX, 3 troff; paragraph 0 is greedy word wrap, 1 min raggedness, 2 Knuth–Plass. Infeasible Knuth–Plass paragraphs report kernel failure.

Use `pp_math_atom`, `pp_frac`, `pp_sup`, `pp_sub`, `pp_root`, `pp_math_row`
and `pp_matrix` for semantic equations. The
[C manual](../../../manual/mulib-prettydoc.md) covers source maps,
incremental rendering sessions and the `.ppg` compiler. The scalar On1x
companion does not expose session handles or map ownership.

Width counts [Unicode scalar display columns](../../../manual/mulib-prettydoc.md#algorithms), not UTF-8 bytes or grapheme clusters.

## Linking

`on1x-build.py` detects the package's raw bridge declarations and selects
`prettysexpr`; the manifest pulls in `prettyemit`, `prettydoc` and the
Knuth–Plass kernel's dependencies. It resolves
`runtime.c` as a stdlib resource and compiles it separately; the planner owns
all kernel switches in one implementation translation unit. No `--mulib` or
`--extra-c` flags are needed.

```sh
python3 on1x/tools/on1x-build.py examples/prettyprint.on1x \
  --on1xc on1x/build/on1xc -o build/pretty-example
build/pretty-example
```

`make -f prettyprint.mk test-on1x` exercises this path; with Exotract installed,
`make -f prettyprint.mk test-exotract` also runs the same tests using a locked
archive, without generating another implementation TU in the application.
Override `ON1XC`, `EXOTRACT`, `ON1X_BIND`, and `BUILD_DIR` as needed.

The companion borrows the input only during the call, retains successful
NUL-free results until process-exit cleanup (16 MiB pool budget, 2 MiB per
output), and returns a static empty string on failure. A successful empty
document also returns an empty string: use the status, not the string length.
The companion is **single-threaded**, and its status must be read immediately
after a render call. Status codes are in [`runtime.h`](runtime.h).

[`prettyprint.exocontract.json`](../../contracts-exoml/prettyprint.exocontract.json)
describes the string/scalar C ABI. `make -f prettyprint.mk test-contract`
extracts the real C declaration, pins the unreviewed annotations to it and
checks ExoML Gate B (declaration/contract consistency). This is **not** safe
FFI admission: the contract does not independently prove ownership or effects,
and the status side channel and process-exit storage are not lowered by the
general scalar adapter generator.

## Direct kernel import status

`import kernel mulib.prettysexpr as PrettyKernel;` resolves in
[KernelBind discovery](../../docs/kernel-bind.md). The separate
[`prettysexpr.exocontract.json`](../../contracts-exoml/prettysexpr.exocontract.json)
describes the *canonical* parser and renderer, not this package's bridge.
`make -f prettyprint.mk test-kernel-discovery` exercises the native import syntax,
real CDecl and ExoML validation, the single-TU plan, and rejection of unsafe or
unadmitted imports during normal compilation.

The package cannot yet replace its raw bridge with this import.
`mulib_pretty_render_expr` borrows an options record, takes a C enum, and
returns a nullable, caller-owned string that must be freed. `mulib_pretty_parse_expr`
has ownership-bearing output parameters. KernelBind does not lower those
shapes or authorize the unreviewed contract. Keep `import std.prettyprint`
for working On1x programs until the renderer can be called through a checked
interface that handles its input borrows, null failure and result cleanup.
