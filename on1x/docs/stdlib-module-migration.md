# Standard-library module migration

The migration inventory tracks actual source files and source-bundle roles,
separately from the SML Basis API catalog. Run:

```sh
python3 -B on1x/tests/stdlib_module_inventory.py --check
python3 -B on1x/tests/stdlib_module_inventory.py > stdlib-module-inventory.json
```

The JSON lists every implementation-format source, package initializer,
interface-format source, and independent fixture, with its package role,
conventional path spelling, and declared member name when present. A
conventional spelling alone is not a compiler admission claim (for example,
`base/fn.on1x` uses a quoted import because `fn` is reserved). A source record
can describe an implementation or a facade which re-exports another file.
A fixture is never a library implementation.

## Baseline at `0d7f9210`

| Profile | Sources | Migration disposition |
|---|---:|---|
| Restricted standard-library bundle | 210 | Keep declared paths and roles until named, quoted and relocated consumers all pass. |
| Separate `sdk/` source layer | 11 | Not part of the restricted standard-library bundle. |
| Legacy daemon KernelBind implementation and initializer | 2 | Keep the KernelBind route; the restricted bundle already ships the separate `daemon.lifecycle` module. |
| Empty MCP initializer | 1 | Source-tree placeholder, not shipped in the restricted bundle or counted as migrated functionality. |
| Test programs | 33 | Keep as independent inputs, not exported modules. |

There are 50 `__init__.on1x` sources across the profiles, and no `.on1xm` or
`.on1xi` stdlib sources at the baseline. The 186 Basis catalog entries, including
60 partial executable structure bindings, are tracked independently;
metadata-only components are not candidates for stub generation.

## Compatibility gates

- Preserve existing `std.*` names, the standalone `mulib` manifest name,
  facade selection and re-export spellings, and relative/quoted source
  paths. For instance, `std.parsec`, `std.parsec.parsec` and a quoted path
  to `parsec/parsec.on1x` must still reach the same implementation.
- Keep package initializer visibility as it was. A child import does not
  implicitly load its ancestor initializer. Native/resource companions may
  be absent intentionally from a portable initializer.
- Preserve pervasive `Option` and `Result` identities separately from the
  existing tagged-tuple and S-expression APIs. Preserve raw extern names,
  link plans, and the `sexpr`, `visitor` and `ppx` consumers.
- Verify any replacement via source-tree, strict-C11 executed consumers and
  the relocated source bundle/SDK wherever supported. Show that old and new
  names do not duplicate implementations, including nominal identities where
  applicable. Verify checked interfaces reject implementation drift before
  describing them as live interfaces.

Inventory and baseline compatibility tests do not migrate a source or promote
any SML Basis conformance status.

## Canonical implementation batch

| Declared member | Canonical source (with sibling `.on1xi`) | Checked public functions |
|---|---|---:|
| `std.base.ordering` | `base/ordering.on1xm` | 3 |
| `std.http.http` | `http/http.on1xm` | 5 |
| `std.rpc.rpc` | `rpc/rpc.on1xm` | 2 |

The restricted bundle certifies these three members in its root `On1xfile`.
They keep their existing bodies and public signatures. The sibling signature
is mandatory and checked on ordinary use. Old same-stem `.on1x`
imports within the selected standard-library roots are compatibility paths,
not shipped copies or forwarding implementations. Dotted and old quoted RPC
imports are exercised after bundling; changing the signed implementation's type
fails even on the ordinary C emission path.

Parsec stays headerless with its original build-time v1 interface. This keeps
the existing `stdlib_extensions.on1x` Unit-v1 PPX route working without
authorizing that protocol to transform checked signed modules.

There are 210 shipped implementation files (three named and 207 legacy), three
shipped named interfaces, and 33 independent executable/negative fixtures.
All package initializers remain legacy. Their selective re-exports and
submodule aliases still select the original targets; the supported signature
form requires local definitions, and cannot yet certify those forwarding
members without changing their identity. Raw extern declarations, callable
parameters, and exposed nominal representations also remain outside this
batch. In particular, the native HTTP client and the pervasive, `sexpr`,
`visitor`, `ppx`, and standalone `mulib` routes are unchanged. Importing a
signed member through a legacy alias still enters a checked module boundary;
Unit-v1 PPX cannot transform such a graph. These counts do not imply added
SML Basis conformance.

The shipped `std.daemon.lifecycle` implementation uses dot projection for
its budget and retry records and static constructors for the pervasive
`Option<DaemonJobId>`. The existing `daemon/fixtures/lifecycle_types.on1x`
continues to exercise bracket projection and `variant` from a downstream
consumer. This is a syntax-only change; it does not migrate the lifecycle
module, change its nominal types, or add a dependency on KernelBind.
