# `on1x-sdk`

`on1x-sdk` is a deterministic, data-only SDK inspection utility. It reports the
implemented ABI/capability subset and inspects or validates bounded JSON,
On1xfiles, and lockfiles without loading extension artifacts.

```sh
on1x-sdk info
on1x-sdk inspect catalog.json
on1x-sdk validate catalogue catalog.json
on1x-sdk validate contract clamp.exocontract.json
on1x-sdk validate on1xfile On1xfile
```

JSON output is key-sorted and compact by default. `--pretty` changes only
whitespace. On1xfile/lock validation reuses the canonical project parser, and
diagnostic catalogues reuse the compiler catalogue validator. Validation is not
capability admission, contract review, foreign safety evidence, or authority to
execute a referenced artifact. The extension-manifest validator checks the
bounded declarative envelope only; the core host still owns dependency
resolution, capability grants, activation, and lifecycle.
