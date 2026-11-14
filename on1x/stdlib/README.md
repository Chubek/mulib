# On1x standard library and native authoring tools

The implemented vertical slice exposes `println`, `assert` and the MuLIB JSON
pilot as compiler-recognized functions. There is **no** installed generalized
On1x ML module standard library yet. Future stages will provide Core, Option,
Result, IO, JSON, Vec and Parsec modules built from reviewed ExoML contracts
and Chisel/Wetstone safety semantics. See `../docs/roadmap.md`.

`sexpr/sexpr.on1x`, `visitor/visitor.on1x` and `ppx/ppx.on1x` implement the
native codec, walkers, schema JSON loading/validation/extensions, transformer
entry point, constructors and fresh names in **On1x itself**. C11 primitives
provide persistent reference-counted syntax values, byte buffers and bounded I/O.
They use ordinary file imports, not a new module system. Python `__init__.py`
versions remain bootstrap/reference implementations.

See [native APIs/ownership/limits](../docs/selfhosting.md), the
[protocol manual](../docs/metaprogramming.md), and [`../stdppx`](../stdppx/README.md)
for six native processors. A self-hosted compiler, general ADTs/modules, IPC and
reviewed safe MuLIB modules remain future work.
