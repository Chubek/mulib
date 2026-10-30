# ocicontainernetwrork

Compatibility spelling retained exactly as requested. The type aliases
`mulib_ocicontainernetwork_t`; `init` forwards to canonical IPv4 plan validation.
New code should prefer [ocicontainernetwork](mulib-ocicontainernetwork.md). Both
headers may coexist; implementation switches remain explicit and distinct.

No network changes/isolation are performed. Returns 1 valid, 0 invalid.
Switches: `MULIB_IMPL_OCICONTAINERNETWRORK`, `MULIB_IMPL_OCICONTAINERNETWORK`,
`MULIB_IMPL_INET4`, `MULIB_IMPL_PARSEC`.
Example: `examples/modules/ocicontainernetwrork.c`.
