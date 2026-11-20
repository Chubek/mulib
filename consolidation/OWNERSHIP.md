# Ownership and execution boundary

- A newly initialized owner is unique until an explicit supported move or
  retain operation. Destroy at most once through the allocator/provider that
  created it. Failure does not create a live owner unless documented otherwise.
- Borrows never transfer ownership. Shared borrows exclude mutation; exclusive
  borrows exclude other overlapping access. Drop, relocation and reallocating
  mutation invalidate the corresponding views. Keep existing lexical rejection
  for any flow not proven by the canonical checker. For a named exclusive
  place, `ref_replace(place, next)` holds its loan across evaluation of `next`,
  then exchanges the value and returns the old one. A typed early return during
  evaluation leaves the original slot intact.
- Recoverable cleanup follows actual language return/scope/deferral semantics.
  Terminal traps do not acquire unwinding semantics from this plan. A native
  callback may not retain a call-scoped view or propagate a C++ exception
  through C.
- A foreign binding must bind exact declarations, target representation, source
  and dependencies, lifecycle and effects. Discovery, generation, execution
  under an explicit raw profile, and safe admission are distinct states.
- Public On1xRT copies descriptor metadata; callback code/context remain
  provider-owned. Arguments borrow handles for the call; successful managed
  results own one handle. Do not transfer private generated-program pointers or
  borrow unpinned local storage across a suspension or thread boundary.
