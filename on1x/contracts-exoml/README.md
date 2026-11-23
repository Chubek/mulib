# MuLIB semantic contracts for On1x

Implemented source annotations and verification tooling:

- `json.exocontract.json`: all 20 core JSON functions;
- `vec.exocontract.json`: all 7 byte-vector functions;
- `parsec.exocontract.json`: all 18 borrowed combinator functions;
- `elvi-scrape.exocontract.json`: all 6 provider/extraction/HTTPS functions;
- `mune.exocontract.json`: all 52 Lua runtime/frontend/C integration functions,
  including GC roots, retained coroutine-thread callbacks and AOT entry lifetimes;
- `quic.exocontract.json` and `http3.exocontract.json`: all 20 codec and native
  client functions, including input/client-borrowed views, output ownership,
  and blocking network/trust obligations;
- `catalog.json`: exact canonical header/manual pins and implementation recipes;
- `obligations.json`: source evidence, branch/lifetime preconditions and missing
  field-sensitive/ownership/error/callback lowering.

**Every shipped sidecar is annotated and unreviewed.** No independent review,
safe On1x import, or proof of foreign C behavior is fabricated. Pin/check never
promotes these annotations to safe export. `emit-safe` remains blocked even for
reviewed metadata.

```sh
python3 on1x/tools/on1x-contracts.py list
python3 on1x/tools/on1x-contracts.py lint
python3 on1x/tools/on1x-contracts.py build-plan json vec parsec elvi-scrape mune
python3 on1x/tools/on1x-contracts.py build-plan quic http3
```

`list` uses the same `MANIFEST.yaml`-backed kernel inventory as
[`on1x-exotract.py list`](../contracts-exotract/README.md). It distinguishes
shipped semantic sidecars, core snapshot profiles, and headers with no sidecar.
`sidecar_header_matches` compares the sidecar's recorded revision to the
canonical header; it is not semantic validation. Non-kernel sidecars (such as
the PrettyPrint native runtime companion) are listed separately. `lint`,
`build-plan`, and `snapshot` still apply only to the explicit core profiles.

CDD entries have no semantic sidecar or snapshot profile. The SDK 2 accessor
adapter in `../contracts-exotract/include/mulib-cdd-accessors.h` validates the
catalogue/plan before exposing count-explicit borrowed views. This removes
unchecked fixed-array access; it does not attach a semantic contract or make a
CDD discovery record callable.

Real Gate B acceptance requires compiler-generated ExoML CDecl artifacts, not
the lint result. The optional SDK-backed `on1x-bind` implements extraction,
lossless public-IR transport, pin/check/explain and narrow raw scalar adapters;
`on1x-contracts.py` creates/rechecks complete source/toolchain/artifact snapshots.
See [the manual](../docs/contracts.md) for commands, provenance, bounds and
remaining safe-import gates; [the C example](../examples/contracts/README.md)
shows actual ownership and exactly-once implementation switches.

Mune pins the changed header/manual plus its runtime, allocation-failure and
Lua differential test sources through `evidence_sha256`. Snapshot CDecl provenance
and Exotract archive locks include these evidence inputs; drift requires another
semantic audit, not an automatic hash refresh. Its explicit
`active_optional_macros` enables `MULIB_IMPL_PARSER`, required by the full runtime.
This does not enable the owned parser for the standalone Parsec profile.

ElviScrape additionally pins its shared scanner, provider data, HTML fixtures,
allocation-failure tests, fuzz harness and raw test adapter as evidence. Its
fixture-only plan uses the same narrow recipe and required exports; the C math
library dependency of the complete one-TU archive is explicit. No independent
TLS/DNS verification or safe ownership/effect lowering is inferred from tests.

The canonical versioned schema and model are in the separately supplied
LibStone checkout's `moonstone/exoml/contracts/schema.exocontract.json` and
`moonstone/exoml/include/exoml/Contract.hpp`. On1x preserves the full model rather
than projecting into the lossy legacy ownership contract. LibStone and MuLIB
kernels are read-only inputs to this workflow.
## Directory binding

This workflow lives in `on1x/contracts-exoml`; `on1x-contracts.py` resolves its
catalog and sidecars here. Exotract static build profiles and archive generation
are documented in [`../contracts-exotract/README.md`](../contracts-exotract/README.md).
