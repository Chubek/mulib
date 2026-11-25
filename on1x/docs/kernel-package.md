# On1x load-time kernel package v1

A load-time application records an independent, typed requirement for each
kernel dispatch slot. A package supplies a checked recipe candidate, not
authority to change the application's contract. This profile distinguishes
the wire format version, the requested module version, and the native adapter
ABI. They are not interchangeable.

## Application requirement

The generated C11 `struct on1x_kernel_import` v1 contains a canonical module
name, export name, exact module version, arity, interface SHA-256 and behavior
SHA-256. The index in the generated array is the dispatch slot. Digests in this
record are NUL-terminated 64-character lowercase hexadecimal ASCII strings.
The generated source also requires `ON1X_KERNEL_IMPORT_TABLE_ABI_VERSION`
to be `1` when compiled against the runtime headers. The C struct is not
serialized by copying its memory representation.

The admitted v1 kernel is `stdffi.clamp` at module version 1: `add_i64` and
`sub_i64` accept two signed `i64` arguments, and `i64` accepts three. All return a
signed `i64`. No alias or C symbol name participates in the interface digest.
The module version is compared separately. Hash the following exact ASCII
bytes, with LF at the end of every line, including the last, and no NUL:

```text
on1x.kernel.interface/v1
module=stdffi.clamp
export=add_i64
signature=(i64,i64)->i64
```

Substitute the canonical names and, for arity 3, use
`signature=(i64,i64,i64)->i64`. For the example above the digest is
`5392fbd1887fc6d83d13d6f1e72ca872d84dace041f0f0e50f140e3ffc3fe1a4`.

The behavior digest independently hashes the checked guard and body of a
single export. The preimage is ASCII `(guard EXPR)\n(body EXPR)\n`, with
literal LF bytes and no NUL. Canonical expressions use `true`/`false`,
signed base-ten `i64` literals without leading zeros (zero is `0`), zero-based
positional parameters `$0`, `$1`, `$2`, and parenthesized operators and
operands separated by one ASCII space. Operand order and operator spelling are
significant. Parameter spelling, source formatting, source provenance fields,
C symbol selectors, and nonsemantic optimization hints and rewrites are not
hashed. Directives must pass the recipe parser's syntax checks and are never
instructions to change the checked expression. Excluded selector and
provenance fields do not establish authorization. Digests cover the checked
expression, not observational equivalence of different expressions.

## Package wire encoding

A package is one length-delimited file, conventionally named
`<module>.on1xkp`. Every integer is unsigned; `u32be` is four big-endian
bytes. There is no padding, alignment, terminator, checksum trailer,
extension field, or trailing data. A `name` is a `u8` byte length followed by
that many ASCII bytes, without a NUL. Each module name is 1–63
bytes matching `[a-z][a-z0-9_]*(\.[a-z][a-z0-9_]*)+`; each export name is
1–63 bytes matching `[a-z][a-z0-9_]*`. Wire digests are 32 raw SHA-256 bytes
in digest order, not hexadecimal.

| Field, in wire order | Encoding |
| --- | --- |
| Magic | Eight bytes: `4f 4e 31 58 4b 50 0d 0a` (`ON1XKP\r\n`) |
| Format version | `u8`, exactly `1` |
| Kind | `u8`, `1` for portable recipe; `2` for native AMD64 Linux SysV adapter ABI v1 |
| Export, dependency and conflict counts | Three `u8`, respectively `1..16`, `0..16`, `0..16` |
| Module version | Nonzero `u32be` |
| Recipe byte count | `u32be`, `1..65536` |
| Image byte count | `u32be`, `0` for kind 1; `1..4194304` for kind 2 |
| Module | Module `name` |
| Exports | Per export: export `name`, `u8` arity (`2` or `3`), interface digest, behavior digest |
| Dependencies | Per dependency: module `name`, nonzero exact module version as `u32be` |
| Conflicts | Per conflict: module `name` (applies to all versions) |
| Image digest | Present only for kind 2; SHA-256 over the exact embedded image bytes |
| Recipe | Exactly the stated number of bytes containing the checked `ffispec 1` candidate |
| Image | Exactly the stated number of bytes; absent for kind 1 |

The three manifest lists are each strictly ascending by name in ASCII byte
order. Thus duplicate exports, dependencies or conflicts are noncanonical.
Self-dependencies, self-conflicts and a module listed as both a dependency
and a conflict are invalid. The reader caps the entire input at
`8192 + 65536 + 4194304` bytes, validates all declared bounds and exact EOF,
recomputes each interface digest and checks the image digest when present.
Apart from these checks, the structural reader can represent module names
and versions outside the admitted runtime profile; decoding a manifest
does not admit those modules.

## Verification boundary

Loaders must parse and type-check the recipe itself and verify that its
module and complete export inventory agree with the manifest, including each
export's recomputed behavioral digest. The selected package must agree with
the application's independent exact version and per-slot type and behavior
pins before the slot is callable. Resolution failures, including an
incompatible dependency, conflict, or ambiguous candidate, do not grant
permission to fall back to a built-in implementation.

Kind 2 carries the same checked recipe as kind 1 and additionally binds the
image bytes and a fixed native ABI. Its adapter names are derived as
`on1x_kernel_native_<export>`; no package string selects a dynamic symbol.
The adapter returns a C `int` status with `0` for success and accepts the
signature's `int64_t` arguments followed by a caller-owned `int64_t *out`.
An image digest found in its own package detects inconsistency, not
authenticity; native loading requires independent host authorization and
native admission checks. Merely decoding kind 2 never executes it.

The native profile admits little-endian ELF64 `ET_DYN` for AMD64, with no
entry point, executable stack, writable executable segment, dynamic
dependencies or imports, relocations, TLS, or load/unload hooks. The loader
cross-checks the processor, segments, section inventory and dynamic symbols
against LibStone qBFD. It checks the loader-view tables separately: qBFD's
section view alone does not constrain which entries the system loader will
use. It approves and examines a sealed Linux memory file, opens that same
image, and verifies that the callable entry points belong to it. This is
admission for explicitly trusted native code, not a sandbox or a proof of
machine-code equivalence to the recipe.

## Lifetime

Startup holds package bytes in a private resolution plan while validating
the graph and staging checked recipes, native image handles and typed slots.
The verifier outputs do not borrow package memory. On failure the runtime
releases staged slots and provider owners in reverse order and releases the
resolution plan; it publishes nothing. On success it publishes one complete
immutable table, releases the package bytes, and retains the table's checked
recipe and native owners until process exit.
