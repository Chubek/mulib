# Foreign inventory and taint diagnostics

`ffgen` records projected native declarations separately from executable
descriptors. Its index retains candidate signatures and reachable native
types, including excluded or unsupported functions, plus `projection_only`
entries for non-function declarations and functions outside the module
selection. Only local-looking objects and typedefs receive full type
details; other projection-only entries identify the symbol and kind. None of
these entries registers a callable member.

`taint` is a **lower bound on observed foreign boundary hazards**, not a
verdict about a library or proof of a vulnerability. The Cephyr
`--emit-interface=sexp` projection supplies explicit `_Noreturn`, variadic
and non-prototyped signatures, and function-pointer types. These facts come
from resolved types and specifiers, not symbol spellings. Neither a pointer
alone nor an identifier like `variadic` is sufficient. The generator retains
the underlying ownership/representation outcome separately.

An enriched Cephyr analysis report can also supply
`taint_projected_out_of_bounds` when its validated, bounded memory model
disproves safety for the exact projected definition. FFIHandler checks that
the directive and catalogue report agree before registering this diagnostic.
It does not turn `unknown` into a finding, and it does not claim that a
different linked implementation has the same body.

For a bounded body observation, build the optional Wetstone helper:

```sh
cmake --build BUILD --target on1x-wetstone-ffi-taint --parallel 1
```

Pass its absolute path at `BUILD/hir/FFIHandler/on1x-wetstone-ffi-taint` to
`ffgen --wetstone-taint`. The generator invokes real Cephyr
`--emit-ast=sexp` with a controlled translation unit and profile, then
replays those exact AST bytes through `--emit-interface=sexp`. The
generator checks the replayed declaration inventory against the AST.
It qualifies only a function whose first
statement returns a call through a parameter that the interface resolves
as a function pointer. The call's arguments, if any, must be simple
parameter reads. Wetstone's dataflow pass checks that the incoming
parameter reaches the modeled indirect callee value. The observation names
the parameter ordinal and **preprocessed** line, not a resolved C source span.
The helper runs no C code.

The index marks this body analysis `partial` or `unavailable`, never clean.
This does not resolve general C body semantics, reachability, aliasing,
lifetime, ownership, callback retention, or library-wide effects. It does
not inspect generated machine code or independently linked implementations
not present in the projected translation unit. Unsupported syntax, parse
failure, or budget exhaustion provides no negative safety evidence.

## Policy

The compiler and generated native build driver default to
`--ffi-taint reject`. A use of a known tainted member reports
`ON1X-FFI-TAINT`. If a module has no represented bindings, the generated
FFI import preflight may diagnose its first known tainted discovery member
at the import. The index remains available as inventory.

For hosted project FFI, the root `On1xfile` may specify `ffi-taint reject`
or `ffi-taint allow`. The `on1xc aux build|plan --ffi-taint reject|allow`
argument overrides that root setting. Dependency and directory manifests
cannot select the policy. The direct equivalent is
`on1xc --ffi-taint reject|allow` (also forwarded by `on1x-build.py`).

`allow` suppresses only this extra diagnostic. Discovery entries remain
non-callable, unknown members are still rejected, and ABI checks, lexical
`unsafe`, linkage, and independent signed review are unchanged. Catalogue
metadata cannot grant admission. Existing v2 inventory can contribute
structural observations only for exact emitted discovery members whose
`.ffi` file identities match the index; it cannot create a descriptor.
