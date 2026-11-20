# Application workload checks

These are repository-local integration examples, not product ports. Only
Shylock executes an On1x-authored application. The three C11 executables use
public MuLIB mechanisms directly; their results do not imply an On1x import is
approved or even representable. No production credentials or third-party
project builds are required.

| Target | Executed here and checked against | What a product integration must still show |
|---|---|---|
| **Shylock – On1x → C11 + test-reviewed KernelBind** | A three-module application reads local versioned fixture text from stdin and checks product, USD/2, consecutive observation days and bounded integer cents. It compares real EMA/Holt, follows chronological training/selection/holdout separation, reports holdout MAE, refits the selected model for the next observation and feeds that forecast into real PriceOpt as a price limit. Oracles: an arithmetic trend, flat training/selection with nonflat holdout, and a quadratic grid profit with a closed-form optimum. Includes altered holdout, no-demand, invalid currency/format/order, rejected forecast/optimization, and an unsigned invocation that must not publish a bundle. | Use admitted input/storage APIs for production data; preserve currency and exact money semantics across operations; validate competitor observations, fit actual demand and prove pricing parity. The optional demand record supplies **assumed** linear coefficients, not sales, fitted demand or a demand observation. No statsprice, ensembles, dynamic/foreign owners, Shylock deployment, or production admission. |
| **Amb3r – C11 only** | Create a schematic, serialize/parse a deck, resolve a node by name, evaluate a DC point and a fixed-step RC trace, and measure/interpolate the owned waveform. Compare every sample to the closed-form backward Euler solution, bound error against the continuous solution, and verify singular failure leaves the trace buffer unchanged. Optional `ngspice` compares the DC point on the same generated deck. | The admitted import path needs checked index/units adapters and owners/borrows for native netlists and waveforms, explicit resource/time bounds, and approval of actual signatures. Require product-level design → solve → report with verified identity mapping, diagnostics, and explicit rejection for unsupported devices; this is only linear RC, not generic SPICE or mixed-signal/PCB conformance. |
| **JoltFX – C11 only, optional FFmpeg CLI** | Split a clip, ripple the right segment over a gap, resolve a source tick, and rescale NTSC frame ticks to 48 kHz. Use a synthetic padded-row sRGB RGBA8 frame (explicit byte capacity and stride), decode its RGB to linear light, keep straight alpha, grade, composite, and encode a packed RGB24 frame. Check exact RGB fixture bytes, transparent-color exclusion and short-capacity rejection. Optional FFmpeg encodes the rendered frame as one FFV1/Matroska frame, decodes it, and checks exact RGB parity. | A real `.jfx` read/edit/save, JBC1 v1/v2 and SDK/ABI parity, with an approved frame lease, bounded plane views, real decode/seek/export provider, cancellation/rollback, ImmGTK presentation, and real CPU/GPU capability reporting. This fixture has no `.jfx`, FFmpeg provider linkage, decoded input, Vulkan/GPU, project state or audio device. |
| **Ulysses – C11 only, private socketpair** | Scripted, bounded HTTP/1.1 requests pass over a private connected Unix socket pair. MuLIB parses and serializes; a tiny fixture policy checks route/method and a caller-clocked token bucket. Check wire status and binary echo (`a NUL b`), missing route, wrong method with `Allow`, premature EOF, body limit, clock reversal and rate refill; count dispatches to prove rejected requests never reach the handler. | Demonstrate an actual localhost-only Dublin → Weblet → Dublin flow with an approved owner for request/response streams, cancellable bounded transport, deployment/thread/callback lifetimes, backpressure and capability denial. No external listener, live target, credentials, native extension, daemon, TLS, scheduler or isolation is tested here. Existing *explicit raw* `ipc_framed` operations are not a safe HTTP or sandbox callable path; the sandbox archive demonstrates discovery/build, not enforcement. |

## Reproduce

From the MuLIB root on Linux x86-64 with a native C11 compiler, Python 3,
Clang resource headers, OpenSSL (Ed25519), and `ar`. Use the configured
LibStone checkout without changing it. The baseline configuration with
Exotract Lua **off** cannot run the reviewed import test.

```sh
out="$PWD/build/workloads"
sdk="$out/sdk"
libstone=/mnt/nvme/mulib/on1x/third_party/libstone  # or the configured LibStone path
cmake -S on1x -B "$sdk" -DCMAKE_BUILD_TYPE=Debug \
  -DON1X_LIBSTONE_ROOT="$libstone" -DON1X_REQUIRE_LIBSTONE=ON \
  -DON1X_WITH_EXOML=ON -DON1X_WITH_EXOML_EXTRACT=ON \
  -DON1X_WITH_EXOTRACT=ON -DON1X_WITH_EXOTRACT_LUA=ON \
  -DON1X_BUILD_NATIVE_PPX=OFF
cmake --build "$sdk" --target on1xc exotract --parallel 2
exo="$sdk/libstone-moonstone/exotract/exotract"
python3 -B on1x/tests/test_kernel_history_sdk.py "$exo" "$sdk/on1xc" \
  "$PWD/on1x" "$out/baseline" cc
python3 -B consolidation/workloads/shylock/check.py --on1xc "$sdk/on1xc" \
  --exotract "$exo" --artifacts "$out/shylock"
python3 -B consolidation/workloads/shylock/check.py --on1xc "$sdk/on1xc" \
  --exotract "$exo" --artifacts "$out/shylock-san" \
  --cc clang --cflag=-fsanitize=address,undefined --cflag=-fno-sanitize-recover=all \
  --cflag=-fno-omit-frame-pointer
python3 -B consolidation/workloads/check_c.py --artifacts "$out/c-gcc"
python3 -B consolidation/workloads/check_c.py --artifacts "$out/c-san" \
  --cc clang --cflag=-fsanitize=address,undefined --cflag=-fno-sanitize-recover=all \
  --cflag=-fno-omit-frame-pointer
```

The Shylock check obtains its unsigned target-specific subject from the
canonical driver, confirms denial without a review, then generates a temporary
test-only Ed25519 key and signs that exact subject. It deletes the private key
before compiling; the temporary test approval and bundle are removed after the
run. It checks the exports and one-TU archive, then feeds raw fixture bytes to
the compiled executable. No inputs are baked into the On1x source. This uses
the runtime's NUL-free stdin text ABI: NUL is a runtime error before fixture
parsing, and the parser's 256-byte bound applies **after** stdin is read.

Shylock fixture grammar: LF-terminated `shylock-v1,widget,USD,2`, six
`day,price_cents` records and optional `demand,scale,slope,cost_cents`; numbers
have one to six ASCII digits. Success prints newline-delimited positional
fields: `ok`, selected model, EMA MAE, selected MAE, next price, then
`no-demand`, `optimization-rejected`, or `quote` followed by price, objective
score, units and evaluation count. Floats use the runtime's hexadecimal
representation. Application validation failure reports `invalid-fixture` or
`forecast-rejected` without publishing a quote.

Missing `ngspice` or `ffmpeg` is reported as **UNAVAILABLE** for that external
comparison only; the independent C oracles still run. FFmpeg must provide the
FFV1 encoder/decoder. There are no downloads or external network requests.
