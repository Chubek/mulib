# On1x VMWeave backend

`on1xc --emit-vm app.on1x -o app.o1vm` emits a portable, versioned register
bytecode artifact. `on1x-vm --verify app.o1vm` verifies it; `on1x-vm app.o1vm`
loads, re-verifies and executes it. `on1xc --run-vm app.on1x` uses the same
backend in-process. A program exit value comes from a zero-argument `main`
returning i64 or void. Arithmetic/assertion failures return 70; execution budget
exhaustion returns 75. Both preserve stdout already produced and report the
failing source location. `--steps N` and `--depth N` on the standalone runner
set execution limits. The C++ API also accepts typed arguments and tuple results.

## What executes

`On1xVM.lua` is a real LibStone `limestone/vmweave` specification defining 44
stable opcodes: 40 MIR operations and four block/control instructions.
VMWeave's Lua/Kaguya frontend, STK-00 model and C generator produce the switch
dispatch, validation, instruction bodies, tape, control transfers and VM return
stack. The typed callbacks in `vm.cpp` implement operations at already-verified
dense register/slot addresses. Each generated handler statically calls its own
operation; there is no callback that switches over a MIR instruction stream.
Generated `vm_call`, `vm_return` and `vm_jump` advance the real guest PC.
Arguments for a CFG edge are snapshotted before its destination writes any
parameters, including on self-edges. Function values and locals are per frame.
The generated stack machine and typed register state belong to one invocation.

The VM shares the compiler's checked-integer, strict-IEEE and numeric-descriptor
**scalar primitives**. It does not call the reference evaluator, generate or
compile guest C, load executable payloads, or resolve guest-selected native
symbols. Normal builds and executions do not need Lua/VMWeave or a C toolchain
at run time. A source-side test compares against the independently compiled C11
runtime and the scalar MIR reference evaluator.

### Build/regenerate

With LibStone at the pinned `5d733d69f19fac1d8a05294fe54e4a4ec863fc4c`
revision and its dependencies checked out:

```sh
cmake -S on1x/third_party/libstone/limestone -B build/limestone \
  -DLIMESTONE_BUILD_TESTS=OFF -DLIMESTONE_BUILD_TOOLS=ON
cmake --build build/limestone --target vmweave-cli --parallel 4
cmake -S on1x -B build/on1x -DBUILD_TESTING=ON \
  -DON1X_VMWEAVE_EXECUTABLE="$PWD/build/limestone/vmweave/vmweave-cli"
cmake --build build/on1x --target on1xc on1x-vm on1x_vm_tests --parallel 4
ctest --test-dir build/on1x --output-on-failure -R '^on1x\.vm\.'
```

An ordinary build verifies the SHA-256 of the spec and all generated C/STK
snapshot files before using them. An explicit generator regenerates into the
build directory. `regen.py GENERATOR --check` compares a fresh, deterministic
generation byte-for-byte; `regen.py GENERATOR` updates the source snapshot.
The pinned Lua, Kaguya and Perl generators are **build-time** dependencies of
VMWeave, not guest components.

## Wire format and admission

`wire.cpp` is the codec for `O1VM 01 0d 0a`. The next byte selects the
MIR semantics version (1, 2 or 3). A module contains typed function, slot
and block tables. Each block starts with `Enter`, has typed operation records
and ends in a checked `Return`, `Jump` or `Branch`. Operands use canonical
unsigned LEB128 IDs; signed constants and IEEE payloads store their exact 64
bits. UTF-8 or native path bytes are length-prefixed. Tuple types have explicit
arity. Function/slot targets, effect dependencies, operand/result types, source
locations and exact numeric descriptors travel with the code.

Decoding rejects malformed/truncated/overlong integers, invalid types and
opcodes, surplus bytes and out-of-budget input before tape publication.
`mir::verify` then checks signatures, canonical numeric payloads, exact effects,
SSA dominance, initialized slots, target identities, CFG reachability and all
edge/return contracts. Only then does the loader resolve IDs into dense
descriptor and tape indexes. Generated VMWeave dispatch validates the resulting
tape as well. There are no guest-supplied pointers. Bounds include 16 MiB
artifact bytes, 4,194,304 cumulative list entries, depth-32 types, the public
MIR verifier's work/node budgets, 262,144 live typed frame cells, 4 MiB output,
and a maximum depth of 256 calls. Tuple copying is charged against the step
budget. Floating-point operations have the same IEEE/environment admission
as the scalar MIR evaluator (not a portable soft-float implementation).

## Source and MIR coverage

All verified MIR v1–v3 operations and all 155 admitted v3 numeric descriptors
are implemented for the supported execution environment and finite budgets.
`tests/vm.cpp` exercises each operation, including checked signed operations,
unsigned checked/wrapping/bit conversions, float32/64, nested tuple arguments
and results, mutable slots, calls and parallel phi transfers. Malformed branch
IDs, operand types, effect tokens, calls, truncated bytes and budgets have
negative tests. Tests are `on1x.vm.opcodes`, `on1x.vm.source`, and (with the
generator configured) `on1x.vm.regeneration`.

The VM source route goes beyond ordinary scalar MIR:

* The VM-specific `lower_for_vm` projects finite copyable record/sum/array/slice
  representations into typed products, under its documented layout bounds.
  With `memgraph-bounds`, fixed-array accesses omit only proved redundant MIR
  assertions, preserving operand order. Dynamic and slice-backed accesses stay checked.
* `hir/vm_closures.cpp` lifts immediate lambdas, including canonical
  legacy match arms with product arguments, into named bytecode functions.
  First-order function **values** use finite tag-and-capture products and typed
  per-signature dispatchers. Named function alternatives, immutable captures,
  returning a closure and passing a callback to another function are executable.
  Callee evaluation precedes arguments; captures are snapshotted at creation,
  with no C function pointer or host heap closure in the artifact. The checked
  source and converted private HIR are both verified. Generics/macros already
  erased by the authoritative frontend do not require runtime VM opcodes.
  Capturing another closure, embedded function-bearing aggregate values, unknown
  callback families and incompatible capture layouts fail closed. Nonuniform
  alternatives currently support padding only scalars/tuples; their closed
  signatures have at most 128 targets and 256 capture fields.

**Not the entire On1x source language.** Current MIR has no general GC/RC object
identity, foreign ABI or raw-call authority, ownership/borrow/drop events,
structured cleanup, suspending continuation, channel or scheduler descriptors.
Strings/managed resources, general higher-order recursive environments, staging
with live code values, dynamic defer, CSP and arbitrary externs cannot generally
be compiled through these typed products. A verified extension needs a versioned
managed-value/object representation, explicit allocation/loan/move/drop and
cleanup transitions, followed by capability-qualified imports and checked task
frames. VMWeave's object/module/IPC components alone do not supply the On1x
language's ownership, FFI and concurrency proofs. Such programs must not
silently fall back to native C or the MIR evaluator.
