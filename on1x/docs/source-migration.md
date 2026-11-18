# First-party On1x source migration

The audit covered 229 tracked `.on1x` files: 54 examples, 135 stdlib
files (including tests), 25 stdppx files, and 15 other test files.
It excludes vendor code and the LibStone submodule.

## Native kernel imports

The clamp examples use native imports for all three functions in the v2
profile. `base_clamp_i64` in `stdlib/base/ordering.on1x` remains a portable
On1x implementation: switching to `Clamp.i64` would require an explicit
KernelBind build and review for ordinary standard-library consumers. Both
check the ordering of the bounds. Pricing and forecasting sources were
outside this migration.

The profiles below are not callable with either import spelling. In
particular, discovery does not replace a linked adapter; see the
[staged fixtures](../tests/fixtures/kernel_migrations/README.md) for prospective
projections and the [KernelBind manual](kernel-bind.md) for authorization.

| Existing consumer (paths relative to `on1x/`, except as noted) | Native C declarations and migration blocker |
|---|---|
| `stdlib/prettyprint/prettyprint.on1x`: `pp_render`, `pp_format`; root `examples/prettyprint.on1x` and `tests/prettyprint.on1x` | `mulib_pretty_options_init`, `mulib_pretty_render_expr`: initialized options record, input/record borrows, target enum, nullable owned C result, `free`, and status and output-limit parity. |
| `stdlib/json/{json,normalize}.on1x`: `basis_json_array_length`, `json_valid`, `json_normalize`, `basis_json_normalize`; `examples/stdlib_extensions.on1x` | `mulib_json_parse`, `mulib_json_destroy`, `mulib_json_typeof`, `mulib_json_size`, `mulib_json_stringify`: exact text length, nullable owner, checked counts and compact serialization with owned-copy/free and recoverable status. |
| `examples/{json,memory}.on1x`: `json_parse_or_die`, `json_get_or_die`, `json_size`, `json_print` | The JSON intrinsics additionally depend on `mulib_json_get`. They need a lifetime-checked child view and their existing cleanup and error guarantees. The checked runtime path remains. |
| `stdlib/{yaml/yaml,toml/toml}.on1x`: `yaml_as_json`, `toml_as_json`, `basis_yaml_as_json`, `basis_toml_as_json` | `mulib_yaml_parse`, `mulib_toml_parse`: shared JSON ownership and serialization; the `parsec`/`peg` manifest cycle also blocks current discovery. |
| `stdlib/ini/ini.on1x`: `ini_has`, `ini_get`, `basis_ini_get`; `examples/stdlib_extensions.on1x` | `mulib_ini_parse`: scoped synchronous callback and userdata; copy transient strings, retain the first match, validate trailing input, and preserve distinct missing, empty, parse-failure and allocation-failure results. |
| `stdlib/crypto/crypto.on1x`: `crypto_blake3`, `crypto_blake2s`, `basis_crypto_blake3`, `basis_crypto_blake2s` | `mulib_blake3`, `mulib_blake2s`: borrowed bytes, 32-byte output buffer, checked status and success-only hex encoding. |
| `stdlib/socket/socket.on1x`: `socket_connect`, `socket_send`, `socket_recv`, `socket_close`, `basis_socket_receive`, `basis_socket_receive_hex`; `stdlib/network/network.on1x`: `network_send_all` | `mulib_socket_connect`, `mulib_socket_send`, `mulib_socket_receive`, `mulib_socket_close`: checked ownership/borrows, initialized descriptors, partial I/O and transfer counts, IPv4 validation, stale handles, AGAIN/EOF and binary-vs-text status parity. |
| `examples/cooperative_channels.on1x` and other cooperative examples | `mulib_channel_create`, `mulib_channel_sender`, `mulib_channel_receiver`, `mulib_channel_send`, `mulib_channel_receive`, `mulib_channel_close`, `mulib_channel_destroy`, `mulib_channel_select`: directional/scoped endpoints, `i64`/byte storage, cancellation, atomic choice and cleanup belong to the checked CSP runtime. The staged blocking channel projection is not a scheduler substitute. |

`stdlib/mulib/Native.on1x` and `examples/mulib.on1x` use housekeeping
tables generated at build time, not an API of any one kernel. The Basis
collection, numeric, filesystem, I/O, process and time adapters use dedicated
companions or libc/POSIX. The raw `extern fn` examples `external.on1x` and
`evaluation.on1x` exercise application-defined C symbols. None is a KernelBind
replacement candidate.

## Compatible control-flow updates

Ascending `i64` counters with stable bounds use ranges; the Base64
three-byte stride keeps a checked three-clause `for`. Fresh-name searches
return a value from `loop`, and byte-escape dispatch and the value-data example
use structural `match ... with`. Traversals whose stopping condition depends
on changing state still use `while`, including list tails, parsing cursors,
network offsets, and reverse/countdown loops. Intentional syntax and backend
regression fixtures keep their original forms.

## Bounded CSP examples

`examples/cancellation_deadlines.on1x`, `examples/cooperative_process_streams.on1x`,
and `examples/cooperative_stream_poll.on1x` use `taskgroup within 5s` to bound
the whole demonstration, including the child tasks and captured process
streams. All explicit handles are still consumed by `join` or `cancel` on
normal exits. In the cancellation example, `poll after` still selects a local
result; a group deadline instead drains cooperative cleanup and terminates
the run. The process-stream example uses `loop` and breaks on EOF.

`examples/cooperative_channels.on1x`, `examples/cooperative_poll.on1x`, and
`examples/cooperative_external.on1x` retain plain `taskgroup` for the
unbounded rendezvous, atomic choice, and explicit process cancellation demos.

See [verification](../VERIFICATION.md) for commands and measured results.
