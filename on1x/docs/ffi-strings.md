# M2 slice: reviewed immutable C-string admission

`on1x-bind emit-safe` extends the existing four-gate scalar admission with
`reviewed-immutable-string/v4`. This is a narrow, contract-backed profile, **not
general safe FFI and not certification of the standard MuLIB bridge**. Existing
v1/v2/v3 artifacts retain their original meanings and cannot authorize strings.

## Admitted boundary

- Exact, extracted direct `const char *` only. C11 `_Generic` checks the real
  function prototype; size/alignment assertions check the extracted pointer ABI.
  Pointer typedefs, mutable/volatile/restrict pointees, wide text, arbitrary byte
  buffers, pointer arithmetic, callbacks and aggregate ABI remain rejected.
- Parameters: non-null `borrowed_immutable`, `encoding: nul_terminated_bytes`,
  lifetime `{origin: explicit_region, region_name: call}`. No retention, mutation,
  registration, transfer, length-pair or alias obligations may cross this call.
- Results: non-null `borrow` with `{origin: global_static}`, no alias,
  deallocator or invalidation. Review must establish **immutable, valid,
  NUL-terminated storage until process exit**, including across later calls.
  Returning an input alias, scratch buffer or allocation needing caller cleanup
  is not supported. A `const` C type alone does not establish these properties.
- Calls are single-threaded and nonsuspending. Allowed effects remain exactly
  `read`, `write`, `io`, `foreign_call`; unknown, allocation/free, escape, thread
  and suspension effects fail admission. No resource cleanup is invented.
- Strings contain NUL-free bytes, not necessarily UTF-8. No Unicode validation,
  character-index semantics or embedded-NUL transport is implied.

Generated adapters check inputs against 2 MiB and results against 4 MiB,
excluding NUL. Null or limit failure terminates with status 72 before the caller
uses a result or the foreign function receives an invalid input. This is a
terminal boundary failure, not recoverable `Result` lowering. The bounded scan
**cannot validate arbitrary pointer provenance**: review must establish readable
NUL-terminated storage. The adapter neither copies nor frees borrowed storage.

## Contract and use-site gates

The existing command executes every gate, all-or-nothing:

```sh
on1x-bind emit-safe declarations.json reviewed-contract.json output/import \
  --on1xc BUILD/on1xc --cc cc --workdir output/fresh-gates --impl implementation.c
```

Review provenance must already exist; pinning deliberately clears review status.
Gate eligibility rejects unsupported obligations, Chisel checks the actual
generated On1x facade, and strict target C11 compilation/execution checks the
boundary harness. The manifest records input identity, contracts, allowed effects,
commands and observations. Empty string arguments are used for representative
gate calls; result observations record byte length so newlines cannot corrupt
the line protocol. Representative execution is not a foreign semantic proof.

The companion `on1x.admitted-extern/v4` artifact uses **directional** types:

```text
extern on1x_ffi_<fingerprint>_function (borrowed_string,i64) -> static_string
```

Only parameters admit `borrowed_string`; only results admit `static_string`.
Unqualified `string`, swapped qualifiers, legacy schema versions, stale identity
and signature drift fail closed. Consumers must pass the trusted artifact to
`--memory-check --admitted-extern FILE` (or `--check-memory`); ordinary `--check`
does not certify foreign safety. Admission files are explicit trusted build
inputs, not signed certificates. Chisel checks use sites and modeled lifetimes;
it does not inspect the foreign C body or verify declared effects.

## Evidence and remaining blockers

`tests/test_contract_strings.py` performs real libclang discovery and compiles
`tests/fixtures/contract_strings.on1x` through C11 against separately compiled C
and C++ implementations. It compares byte lengths against a seeded Python oracle
and checks stable old results, empty/newline text, nested calls, ABI drift,
review rejection, bad lifetime/effect/ownership metadata, source type errors,
directional evidence, null/limit failures and output nonpublication on failure.
Test review flags are explicitly synthetic and never modify production contracts.

See `../VERIFICATION.md` for configurations and exact results. Standard bridge
safe exports still need independently reviewed pinned contracts plus allocation,
error, foreign-owner and effect lowering for operations outside this profile.
No shipped adapter is relabeled safe; `emit-raw` is unchanged. MIR foreign-call
execution, general owned returns, callbacks and cross-thread string storage remain
unsupported.
