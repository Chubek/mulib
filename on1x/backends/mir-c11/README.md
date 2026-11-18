# `mir-c11` SDK backend

`mir-c11` emits a C11 translation unit from verified MIR using the existing
`on1x::mir::emit_c` implementation. It does not invoke a C compiler, run the
artifact, or extend the set of source constructs that MIR supports.

The compiler's private built-in invocation borrows its in-memory MIR. For a
standard SDK invocation, the adapter reads the canonical program bytes and
decodes them with `on1x::mir::deserialize`, which verifies the decoded MIR.
Both routes call the same emitter (which also verifies) and write through the
SDK output sink; a failed run returns no artifact.

The wire format preserves each MIR location's filename bytes, byte offset,
line and column. Verifier diagnostics refer to those decoded locations, while
syntax/decoding diagnostics refer to positions in the MIR buffer (`<mir>`).
The emitter's own diagnostics and source-location behavior are unchanged.
The private in-memory path avoids the wire's independent byte/node limits.
Decoding also rebuilds shared tuple type objects, which can increase the
verifier's structural-comparison work even when the types are equivalent.
The adapter stores exceptions other than `std::bad_alloc` in its private
per-invocation context so the driver can report the original diagnostic.
`std::bad_alloc` maps to `ON1X_SDK_LIMIT`.

`test_backend.cpp` covers byte-for-byte parity with the direct emitter, source
phase rejection, sink limit propagation, both diagnostic paths, and a verified
module whose wire size exceeds the S-expression limit. To run it manually
from the `on1x/` directory with an existing SDK-enabled build:

```sh
build=/path/to/on1x-build
c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -Iinclude -Ibackends -Isdk/include -Isdk/src \
  backends/mir-c11/Backend.cpp backends/mir-c11/test_backend.cpp \
  "$build/sdk/libon1x_sdk.a" "$build/libon1x_frontend.a" "$build/libon1x_scanner.a" \
  -o "$build/test_mir_c11_backend"
"$build/test_mir_c11_backend"
```
