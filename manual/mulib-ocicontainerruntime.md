# ocicontainerruntime

Explicit-backend lifecycle driver. `init` copies ID and requires an action callback
plus borrowed context. `apply` validates transition/exit code, calls backend with
current state/desired state, then publishes new state only on callback success.
Returns 0 success, -1 invalid, or backend's nonzero result. No automatic callbacks
on object destruction and no implicit backend implementation.

Backend/context must outlive runtime; no reentrancy or runtime mutation during
callback. Callback failure must leave backend state unchanged. Real backend must
perform isolation, process supervision, privileges, mounts, namespaces/cgroups
and security policy. This module alone is **not a usable isolated OCI runtime**.
Switches: `MULIB_IMPL_OCICONTAINERRUNTIME`, `MULIB_IMPL_OCICONTAINER`.
Example: `examples/modules/ocicontainerruntime.c` uses a test-only backend.
