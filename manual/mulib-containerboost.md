# mulib-containerboost

## NAME AND INTEGRATION

Explicit execution-backend registration, capability matching and deterministic
selection. Define `MULIB_IMPL_CONTAINERBOOST` and transitive `container-runtime`
and `imgformat` switches. Example: `examples/modules/containerboost.c`.

## REGISTRY

`init(registry,limit)` creates an empty bounded table. `register` copies a backend
descriptor, including its validated ID, kind, architecture mask, priority and
runtime operations. Duplicate names fail. Context, callback referents and any
configuration referenced by them remain caller-owned.
PAUSE/SIGNAL capability flags require their corresponding callbacks at
registration, so inconsistent descriptors fail with EINVAL before selection.

Architecture bit N corresponds to `mulib_imgformat_arch` value N. Only bits 0–4
are accepted. Kinds are CUSTOM, KVM, OIX, QVM and JIT; kind is descriptive rather
than executable behavior. The operations determine behavior.

`remove(name)` deletes an exact entry and compacts the registry. `free` releases
table storage only. Descriptor pointers obtained by selection are invalidated by
registry mutation. Existing prepared runtimes retain copied operations, while
their borrowed callback contexts must still remain live.

## SELECTION

`select(registry,architecture,required_capabilities,preferred_name)` excludes
architecturally incompatible entries, entries missing any required capability,
and nonmatching preferred names. An optional available callback must return
positive for a candidate to remain eligible. Higher priority wins; registration
order breaks ties. Returns a borrowed descriptor or NULL when none qualifies.

`prepare` selects once and calls runtime initialization with that descriptor.
Failure propagates without choosing another backend. This keeps hardware/policy
selection explicit and avoids silently changing execution semantics.

## RESULTS AND LIMITS

Uses CT errors: EINVAL for invalid descriptors, EEXIST for duplicate names,
ELIMIT for capacity, ENOMEM for growth, ENOENT for removal misses, ENOTSUP when no
backend qualifies. Availability probing may invoke platform APIs but must not
mutate/reenter the registry. Registration does not start containers.

See the [QVM](mulib-container-qvmboost.md), [JIT](mulib-container-jitboost.md),
[KVM](mulib-container-kvmboost.md), and [OIX](mulib-container-oixboost.md) adapters.
