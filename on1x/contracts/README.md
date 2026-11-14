# MuLIB semantic contracts for On1x

The reviewed contract files in this directory are to be authored against
compiler-generated ExoML CDecl graphs. See `moonstone/exoml/manual/semantic-contracts.md`
inside the separately cloned LibStone checkout and its
`contracts/schema.exocontract.json` reference.

No completed `json.exocontract.json`, `vec.exocontract.json` or
`parsec.exocontract.json` is shipped here: fabricating reviewed foreign
ownership contracts would be misleading. See `docs/roadmap.md` for how to
validate them against the canonical MuLIB headers.

The `on1x_json.c` pilot is a **handwritten restricted facade** whose semantics
are derived from MuLIB's actual JSON manual and are covered by an optional
integration test. It does not substitute for ExoML Gate B/Chisel approval.
