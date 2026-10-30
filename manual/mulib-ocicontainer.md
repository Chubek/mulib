# ocicontainer

Container identity/lifecycle model, not a native container engine. `create`
validates/copies a 1..64 byte ASCII alnum/dot/underscore/hyphen ID, excluding
dot/dot-dot. IDs are never interpreted as host paths. CREATED may transition to
RUNNING or DELETED; RUNNING to STOPPED; STOPPED to DELETED. Deleted is terminal.
Stopped exit code must be 0..255; other transitions ignore it.

`can_transition` queries; `transition` changes only allowed state. All return
1 success/allowed, 0 invalid, preserving object on failure. No allocation or
implicit process/namespace/cgroup/mount changes. Runtime backend performs actions.
Switch: `MULIB_IMPL_OCICONTAINER`. Example: `examples/modules/ocicontainer.c`.
