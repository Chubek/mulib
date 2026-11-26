# Standard-library module migration

The migration inventory tracks actual source files and source-bundle roles,
separately from the SML Basis API catalog. Run:

```sh
python3 -B on1x/tests/stdlib_module_inventory.py --check
python3 -B on1x/tests/stdlib_module_inventory.py > stdlib-module-inventory.json
```

The JSON lists every implementation-format source, package initializer,
interface-format source, and independent fixture, with its package role and
conventional path spelling. A path spelling is not a compiler admission claim
(for example, `base/fn.on1x` uses a quoted import because `fn` is reserved).
`__init__.on1x` is the legacy package entry-point implementation, not a checked
interface. A source record can describe an implementation or a facade which
re-exports another file. A fixture is never a library implementation.

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
