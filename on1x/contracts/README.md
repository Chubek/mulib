# MuLIB semantic contracts for On1x

Implemented source annotations and verification tooling:

- `json.exocontract.json`: all 20 core JSON functions;
- `vec.exocontract.json`: all 7 byte-vector functions;
- `parsec.exocontract.json`: all 18 borrowed combinator functions;
- `catalog.json`: exact canonical header/manual pins and implementation recipes;
- `obligations.json`: source evidence, branch/lifetime preconditions and missing
  field-sensitive/ownership/error/callback lowering.

**Every shipped sidecar is annotated and unreviewed.** No independent review,
safe On1x import, or proof of foreign C behavior is fabricated. Pin/check never
promotes these annotations to safe export. `emit-safe` remains blocked even for
reviewed metadata.

```sh
python3 on1x/tools/on1x-contracts.py lint
python3 on1x/tools/on1x-contracts.py build-plan json vec parsec
```

Real Gate B acceptance requires compiler-generated ExoML CDecl artifacts, not
the lint result. The optional SDK-backed `on1x-bind` implements extraction,
lossless public-IR transport, pin/check/explain and narrow raw scalar adapters;
`on1x-contracts.py` creates/rechecks complete source/toolchain/artifact snapshots.
See [the manual](../docs/contracts.md) for commands, provenance, bounds and
remaining safe-import gates; [the C example](../examples/contracts/README.md)
shows actual ownership and exactly-once implementation switches.

The canonical versioned schema and model are in the separately supplied
LibStone checkout's `moonstone/exoml/contracts/schema.exocontract.json` and
`moonstone/exoml/include/exoml/Contract.hpp`. On1x preserves the full model rather
than projecting into the lossy legacy ownership contract. LibStone and MuLIB
kernels are read-only inputs to this workflow.
