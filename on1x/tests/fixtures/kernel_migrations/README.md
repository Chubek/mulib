# Staged On1x kernel migrations

These are opt-in ports of existing MuLIB-backed On1x code. They are not
imported by the standard library or built as ordinary examples. They use actual
`import kernel` syntax and call proposed bindings; the proposed On1x surface
must be reconciled with the verified binding output before any port replaces
production code.

Today `on1xc --kernel-imports --no-prelude PATH` can inspect a staged source
and its local dependencies. `on1xc --check PATH` must still reject
unadmitted imports. Inspecting a request or extracting its C declaration is
not evidence that the calls type-check, link, or execute. See the
[KernelBind manual](../../../docs/kernel-bind.md) for discovery commands.
The YAML and TOML requests are syntax-only for now: their manifest closure
includes the `parsec`/`peg` dependency cycle, which the current ExoLib
discovery driver rejects. The other ports have discovery profiles.

Promote a port only when its imports can be compiled and executed through
the canonical single-TU kernel plan and its output, errors, and cleanup match
the original adapter. Do not ship both the raw bridge and the new binding as
an implicit fallback.

## Projection checklist

The names below are proposed On1x projections, not declarations with invented
C prototypes. In particular, tuple returns with a success field are shorthand
for *conditional* publication: a failed constructor never creates an owner
or a readable view. A future binding can use a checked nominal `Result` and
update the staged consumers rather than materialize invalid values.

| Port | Canonical C declarations and required lowering |
| --- | --- |
| [`prettyprint/render.on1x`](prettyprint/render.on1x) | `mulib_pretty_render_expr`, `mulib_pretty_options_init`: build an initialized record, check integer and enum values, use existing width/output limits, copy a nonnull owned result and `free` it. Distinguish empty success, kernel NULL (`-2`), output limit (`-4`), and copy/allocation failure (`-3`). The renderer must not borrow from the existing exit-time pool. |
| [`data/json.on1x`](data/json.on1x), [`data/json_owner.on1x`](data/json_owner.on1x) | `mulib_json_parse`, `mulib_json_destroy`, `mulib_json_typeof`, `mulib_json_size`, `mulib_json_get`, `mulib_json_stringify`. Use exact byte length, a nullable heap owner and a child view tied to its lifetime, checked `size_t` to `i64`, and compact serialization with copy + `free`. The owner example ports values and exit status, not the intrinsic's stderr diagnostics. The numeric extraction builtin also requires a checked finite/integral `f64` conversion; it is not ported here. |
| [`data/yaml.on1x`](data/yaml.on1x), [`data/toml.on1x`](data/toml.on1x) | `mulib_yaml_parse`, `mulib_toml_parse`: nullable owned returns use the *same* JSON owner and destructor as `mulib_json_parse`; borrowed input and optional parse error records do not escape. Neither kernel's supported language subset changes. |
| [`data/ini.on1x`](data/ini.on1x) | `mulib_ini_parse`: scoped synchronous callback and userdata, transient strings copied before return, first-match lookup, continued validation through the end of the source. `has_first` returns false on any failure; `get_first` distinguishes missing key, empty value, invalid trailing input and allocation failure. No native lookup C symbol exists. |
| [`runtime/crypto.on1x`](runtime/crypto.on1x) | `mulib_blake3`, `mulib_blake2s`: borrow bounded input, use a 32-byte output with status checking, use no BLAKE2s key, publish lowercase hex only on success. |
| [`runtime/socket.on1x`](runtime/socket.on1x) | `mulib_socket_pair`, `mulib_socket_send`, `mulib_socket_receive`, `mulib_socket_close`: owner creation from initialized invalid descriptors, checked borrows and transferred counts, and destruction. This stages the existing send/receive/close behavior on a local pair; replacing the adapters also needs `mulib_socket_connect`, numeric IPv4/port validation, binary/hex paths, short writes, and status mapping (notably AGAIN and binary rejection). No implicit DNS, TLS, or scheduler integration. |
| [`runtime/channel.on1x`](runtime/channel.on1x) | `mulib_channel_create`, `mulib_channel_sender`, `mulib_channel_receiver`, `mulib_channel_send`, `mulib_channel_receive`, `mulib_channel_close`, `mulib_channel_destroy`: exact i64 storage, scoped directional views, timeout zero, distinct drained close, and consuming destruction only once borrows are finished. |

The proposed managed copies do not implement the legacy process-exit string
pools or their cumulative quotas. That difference must be explicitly resolved
or documented before promotion. Compiler-managed JSON intrinsics and
structured concurrency remain on their checked runtime paths. Replacing the
scheduler itself is not a source import substitution: it needs callback,
fiber lifetime and cancellation checking, plus atomic select semantics.
`std.mulib` lists build metadata rather than exposing a C kernel;
`std.unix` and the Basis adapters using POSIX/libc are not direct kernel ports.
