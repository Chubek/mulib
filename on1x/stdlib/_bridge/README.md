# MuLIB standard-library bridge — raw scalar ABI

`mulib_bridge.c` calls canonical MuLIB kernels through their actual public
headers. It defines **no** `MULIB_IMPL_*` switches. `on1x-build.py` discovers the
bridge extern declarations, selects required kernel groups, links this adapter
once, and uses the shared one-implementation-TU planner. A locked
`--mulib-archive DIRECTORY` supplies the same kernels without duplicate definitions.
`--mulib-root DIRECTORY` selects an external source checkout after installation.

```on1x
import std.json.normalize as JSON;
import std.ini.ini as INI;
fn main() {
  let (status,text)=JSON.basis_json_normalize(" {\"value\":42} ");
  assert(status==0);
  println(text);
  let (found,value)=INI.basis_ini_get("[app]\nkey=\n","app","key");
  assert(found==0 && value=="");  # Valid empty, not a missing key.
}
```

Build with `python3 on1x/tools/on1x-build.py client.on1x --on1xc BUILD/on1xc -o app`.
No manual adapter source or kernel switches are needed. C-only consumers include
[`mulib_bridge.h`](mulib_bridge.h), compile the adapter with selected
`ON1X_BRIDGE_JSON/YAML/TOML/INI/CRYPTO/SOCKET/UNIX` definitions, and link the
planner's kernel TU/archive. Each group is independently compilable; ordinary
serialization/hash clients do not link socket or Unix wrappers.

## Status and lifetime contract

New `basis_*` wrappers return `(status,text)` for serialization, INI, hashing and
socket reception. `basis_json_array_length` returns `(bool,i64)` and rejects
non-array JSON without exporting an owner/view through the raw ABI. The old
unprefixed string wrappers remain compatible, but callers must explicitly read
`on1x_mulib_bridge_status()` to distinguish their outcomes.

| Status | Meaning |
|---|---|
| 0 | Success, including a valid empty value |
| 1 | Missing INI key |
| 2 | Socket EOF |
| 3 | Socket would block / retry |
| -1 | Invalid argument, handle, offset or JSON array type |
| -2 | Kernel parser rejection (may include kernel-internal allocation failure) |
| -3 | Adapter allocation/registration or serialization allocation failure |
| -4 | Adapter byte/handle budget exhausted |
| -5 | Socket I/O failure or unsupported socket backend |
| -6 | Text receive consumed a payload containing NUL |

Status is per-process, single-threaded and replaced by the next bridge operation.
Read it immediately. Tuple wrappers do this for you. Input strings are NUL-free
and at most 2 MiB. Returned strings borrow immutable storage until process exit:
4 MiB per string and 32 MiB total including allocation overhead. The adapter
releases all snapshots and live sockets at exit; clients should close handles
explicitly. `ON1X_BRIDGE_POOL_BYTES` and `ON1X_BRIDGE_MALLOC` support budget and
allocation-injection tests. No borrowed INI callback storage escapes: the first
matching value is copied, the rest of the document is validated, and failures
release temporary copies. Duplicate INI keys use first-match semantics.

## Socket subset

Connect accepts a numeric IPv4 address and port 1..65535, not DNS/TLS. Generation
handles reject double close and use-after-close, even after slot reuse. At most
64 sockets are active; exhausted generations are retired without integer overflow.
`basis_socket_receive_hex` preserves all bytes as lowercase hex.
`basis_socket_receive` rejects NUL, but its consumed bytes cannot be recovered;
use hex for binary protocols. Receive limits are 1..1 MiB. `network_send_all`
handles partial sends with offsets into the original payload, avoiding a retained
slice for each retry; it stops on failure or after 4,096 attempts. Both network
calls and the numeric connect may block: no timeout/cancellation API is claimed.

Unix process helpers are POSIX-only and separately enabled. NNG, RPC, IPC and
LibStone interchange modules remain bounded source-level envelopes/helpers, not
libnng, cross-process messaging, a C++ LibStone ABI or a general callback FFI.

## Trust and testing

This is a **trusted raw adapter**, not an ExoML-reviewed safe import. No arbitrary
pointer, C record, callback or ownership proof is inferred from these externs.
Parser subset/UTF-8 behavior comes from MuLIB. General f64/record/slice ABI,
recoverable foreign-owned resources and safe-import gates remain compiler work.

`on1x.stdlib_extensions` executes real JSON/YAML/TOML/INI and BLAKE2s/BLAKE3,
loopback binary sockets, EOF/stale handles, parser/type errors and allocator/budget
failure cleanup. `on1x.exotract` also builds the bridge's kernels through real
Exotract/ExoML and runs a standard-library archive consumer. Linux is the measured
platform; no cross-target verification is claimed.
