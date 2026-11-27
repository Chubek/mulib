# Opaque module values

The executable opaque profile carries monomorphic copy values through a
verified module signature. Its representation must be a record or enum built
from numeric scalars, booleans, characters, and finite products or sums.
Managed values, owning fields, generic type arguments, and transparent type
equations are not part of this executable profile.

The loader resolves a seal to the original provider's nominal identity.
Import spelling, selective aliases, re-exports, and generic specialization
do not mint a new identity or acquire authority over the representation.
Client code can pass, return, store, and copy a sealed value. It cannot
construct it, project its private fields, select or match its enum tags, or
compare it with `==` or `!=`. A provider can explicitly export construction,
observation, and equality functions. Patterns can bind a whole opaque value
without inspecting it, including when nested inside a transparent wrapper.

Representation access is checked using host-retained function ownership, not
source locations. This also applies after macro expansion and checked
specialization. Unit-v1 PPX cannot carry or restore the authority; its
module-scoped rewrite restriction is unchanged.

Direct C11 lowering is the supported route: it uses the provider's concrete
layout within a single compilation and does not make that layout available to
client source. Emitted C is a compiler artifact, not a separately consumable
opaque ABI header. MIR-C11, MIR-VM, and native MIR fail closed.

After building `on1x_opaque_module_tests`, run
`ctest --test-dir <build> -R '^on1x\.opaque_module$' --output-on-failure`.
This standalone frontend test supplies the same owner maps as the loader,
exercises rejected client and forged-wire/HIR operations, and compiles and
executes the successful C11 program.
