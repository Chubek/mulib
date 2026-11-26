# Nominal-owner MIR: experimental verifier profile

This is the contract for a separately admitted canonical MIR verifier profile.
It is not a source-lowering, public wire, SDK, VM, FFI, or native-backend
support claim. Existing profiles (MIR v1–v4) do not admit nominal owners.

## Nominal identity and layout

The module version is 5 (`nominal_owner_format_version`). A module
contains a `nominal_owners` table of `NominalOwner` declarations, each with a
nonzero `id`, a `name`, a `field`, a `payload` type, and a source location.
Declaration IDs share the global identity namespace with functions, blocks,
instructions, values, and effect tokens. An operation's declaration reference
is its exact ID in this module: two declarations that happen to share a
layout are not interchangeable. The qualified name is unique within the
module and records the nominal declaration, not a foreign or linker symbol.
It consists of dot-separated ASCII identifiers; a field is one such
identifier. Each name or field is at most 128 bytes.

The only layout is a record with exactly one labeled `i64` at field index
zero. Declaration order is layout order; there is nothing to sort. There are
no arguments, variants, packing, host data-layout assumptions, or destructor
hooks. In particular this does not reuse the VM product layout, which erases
nominal identity, or the HIR `Type::Own` spelling.

## Places, operations, and lifetime

An owner place is the instruction ID of a create or move. It belongs to the
current block and is not an ordinary SSA value. An owning instruction has
`type == Type::Void` because it does not produce an *ordinary* value; it does
not deposit storage in an ordinary scalar register, slot, or tuple.

| Operation | `operands` | `target` | Ownership effect |
| --- | --- | --- | --- |
| `owner.create` (`OwnerCreate`) | One ordinary `i64` value | Nominal declaration ID | Takes a copy of the scalar as the sole field of a fresh owner at this instruction's ID. Requires no live owner. |
| `owner.move` (`OwnerMove`) | The current live place ID | Same nominal declaration ID | Consumes the source place and rebinds the same object to this instruction's ID. |
| `owner.drop` (`OwnerDrop`) | The current live place ID | Zero | Consumes the place and ends the lifetime. There is no result. |

All three operations are ordered `Effect::Memory` instructions, with
`immediate == 0`, no primitive descriptor, and the normal incoming/outgoing
effect-token chain. There is at most one live owner at a time. Another create
is legal only after a drop. An implementation may use fixed local storage;
allocation, placement, and addresses are not observable in this profile.
There is no read, take, copy, borrow, retain,
aggregate storage, owner argument, owner return, or erasure to a scalar.
In particular a future `take` would return the nominal payload, not its
lone field.

## Normal exit and rejections

The v5 profile has only single-block functions ending in `Return`. Entry
parameters are `i64` or `bool`, results are `i64`, `bool`, or `void`,
and slots and calls are absent. A module has at least one declaration and
at least one create. Outside each live interval the ordinary instructions
are the v1 scalar operations other than `Load`, `Store`, and `Call`. Inside
each live interval the whitelist is only `OwnerMove` and `OwnerDrop`; even
an ordinary pure operation is rejected there. Every normal return requires
no live owner and must carry the final effect token. Ordinary traps cannot
run with a live owner. Budget exhaustion is not a guest-language drop:
an implementation may reclaim its host storage without recording a guest
drop or running a guest cleanup.

Verification rejects malformed IDs, locations, declarations, opcodes,
operands, types, effect chains, and instruction fields. Specifically it
rejects unknown or substituted nominal IDs, create with an active owner,
use before create, move/drop of a stale, foreign-block, or foreign-function
place, double moves/drops, an ordinary instruction/return referencing a
place, and any normal exit without explicit drop. Joins, loops, crossing
branches, recursive or generic layouts, FFI, and implicit trap/exception
cleanup are outside this profile.

## Compatibility gate

`verify_nominal_owner(module, limits)` is the named verifier opt-in. It
accepts only v5, not a scalar fallback. The normal `verify(module, limits)`
must still reject v5 and reject owner metadata or operations smuggled into
v1–v4. A version tag, target request, directive, or `Limits` argument is
never an opt-in. The profile shares the existing node/work limits, including
charges for each nominal declaration and the bytes of its name and field.

Existing evaluator/C/CFG entry points, codecs and structural equality,
passes, analyses, specialization, SDK, VM, CLI, and Archon remain on their
existing profile. A separate explicit entry point and evidence are needed
for each owner consumer. The experimental C++ opt-ins
`evaluate_nominal_owner` and `emit_nominal_owner_c` are declared separately
in `<on1x/backend/mir_nominal_owner.hpp>` and take in-memory MIR only. Their
optional observation is host-side qualification output, never a guest
payload read or drop hook. The C11 emitter keeps its existing requirement
for a parameterless `main` with an `i64` or `void` result.

Both bytes and in-memory ingress need evidence:
SDK program creation must not label unadmitted owner MIR verified, linked
providers must not accept it through the borrowed in-memory path, and
existing text, SDK snapshot, and VM bytecode readers must not widen their
schema caps. If a private opt-in codec is added, it must independently gate
v5, preserve declaration and place identities, and reject malformed v5
records; its presence does not make it a public artifact format.

Release evidence should distinguish verifier tests from explicit evaluator
and C tests, wire round-trips (if implemented), and rejection through each
legacy boundary. Source lowering from checked HIR and correspondence with
direct C11 ownership are separate gates.
