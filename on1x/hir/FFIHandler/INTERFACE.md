# Scalar descriptor interface v1

`<on1x/ffi_scalar.hpp>` declares `on1x::ffi::ScalarBinding`, lookup methods and C11 emission helpers. The only generated private header is `on1x_ffi_rules.hpp`; the public-facing header contains no dependency on generated files. Entries have module, provider, canonical header, member, C symbol, arity, inline marker, and optional guard operand indices. Tables are immutable and compiler-owned. Pointers returned by lookup refer to static storage.

* `scalar_bindings`: bounded read-only view of all table entries.
* `find_member`: exact module/member lookup; no normalization at the call site.
* `find_scalar`: exact C symbol lookup.
* `scalar_header`: canonical header include.
* `scalar_prototype`: link-only adapter declaration; does not supply or authorize the linked object.
* `scalar_wrapper`: local C11 forwarding definition with exact prototype assertion.
* `scalar_guard`: generates the caller-side guard over already-evaluated compiler-created arguments; verifies arity and indices. No expression is evaluated by this function.

The descriptor manifest schema `on1x.ffi-descriptors/v1` is a build report, not an external compiler input or review token. Its fingerprint hashes canonical JSON (before adding `descriptor_sha256`) including parser, validator, input and row identity. It does not hash linked artifacts or assert the provenance of a parser supplied manually via `--parser`. The CMake integration chooses the real generated parser. Both files are validated before publication; each replacement is atomic but the pair is not a filesystem transaction. Source files are capped at 64 KiB, 64 inputs, 256 exports and 16 arguments per function. These bounds are not a sandbox for the build tools.
