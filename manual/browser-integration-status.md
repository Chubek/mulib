# Native headless browser integration status

Baseline: `a370cd51a843683b8d44500040274f58f5ea8681`. The checked-out
source, not the older comparison SHA in `MANIFEST.md.additions`, is the
integration baseline. The ledger describes required work, not supported APIs.

The checkout has 2,283 public headers; `MANIFEST.yaml` lists 2,059. All 224
ledger headers exist and none have canonical entries, dedicated manuals, or
standard-path standalone examples. Consequently, an upstream build or test
pass is not evidence that those headers are integrated. The existing-kernel
table maps to 64 named headers and manuals; the MicroJS collector is an
additional internal audit item, not a separately named public header.

## Dependency and ownership plan

Arrows point from prerequisite to consumer. This is the intended integration
graph, not a claim about current header includes.

```text
allocation, bounded buffers, UTF/encoding
  -> JS runtime -> WebIDL bindings
  -> native DOM -> HTML tree construction
  -> URL/IDNA/MIME and authenticated transport -> bounded Fetch
isolation, peer authorization, and resource limits -> Fetch and browser host
JS runtime + native DOM -> WebIDL wrapper identity and tracing
HTML + WebIDL + Fetch + deterministic scheduler -> browser host
browser host + committed DOM/resource changes -> renderer streams
native DOM -> CSS/computed style/layout -> renderer streams
Fetch + security policy + structured clone -> storage and workers
JS runtime + WebIDL + resource limits -> Wasm/JS bridge
browser host + capability broker -> optional device providers
```

| Wave | Family owner | Owns |
| --- | --- | --- |
| P0 | JS-core | MicroJS, its collector, MiniVM, jobs and modules |
| P0 | HTML | Tokenizer, tree construction, bridge and parser continuation |
| P0 | DOM | Native tree, selectors, ranges, mutations, and native event semantics |
| P0 | WebIDL | Compiler/runtime, conversions and realm-local wrappers, including `jsdom` |
| P0 | Browser host | Engine, document load, navigation, event loop and timers |
| P0 | Network | URL, encoding, HTTP, Fetch, HTTP cache and cookie machinery |
| P0 | Security | Origin and realm policy, actual-peer authorization, isolation and limits |
| P0/P1 | Streams | Snapshots, typed records, sequencing and replay; producers remain with their owning families |
| P1 | CSS | CSS parsing, computed style and deterministic nonvisual layout |
| P1 | Web-core | Streams, clone, messaging, codecs and general Web APIs |
| P1 | Storage | Persistence, IndexedDB, CacheStorage, quotas and OPFS |
| P1 | Workers | Worker/worklet lifecycle, agent clusters and service-worker interception |
| P1 | Wasm | Validation, execution and JS bridge |
| P2 | Providers | Optional device/media implementations and permission-gated host interfaces |

The coordinator owns shared and generated files, manifest changes and
integration. HTML must not grow a second durable DOM; JS wrappers must not
become a second owner of native identity. Bindings can coordinate both heaps
without making either core header depend on the other. Renderer streams observe
committed state; they do not implement state transitions or gate JS execution.
Family work belongs in separate worktrees and should report the patch,
dependency changes, supported and unsupported cases, and exact test results.

## Initial verification

From the repository root, with the staging ledger available:

```sh
CC=gcc sh kernels/detail/mulib-browser-additions-check.sh
CC=clang sh kernels/detail/mulib-browser-additions-check.sh
make check-web-primitives
```

Both staging runs passed independent strict C11 compilation of all 224
headers, the combined translation unit and nine behavioral groups. Each also
ran the Clang ASan/UBSan smoke test with leak detection. The upstream web
primitives target passed its six suites and 10,378 exact money-reference cases.
These are separate checks, not end-to-end browser evidence. The smoke checker
accepts the ledger path as its first argument when the ledger is supplied
outside the checkout.

## Integrated P0 fixes

| Family | Verified behavior | Still unsupported |
| --- | --- | --- |
| JS-core | MicroJS implements `&&=`, `\|\|=` and `??=` with one target evaluation and no write on the skipped path. Host-driven FIFO jobs retain queued closures through collection, enforce a checkpoint budget, and preserve later jobs after failure. Explicitly posted trusted closures can share the browser's native microtask FIFO. | Automatic draining of standalone VM jobs, ECMAScript Promise reactions and modules, anonymous-function name inference for logical assignments, bitwise compound assignments, and full ECMAScript conformance. |
| Browser host | Free during callbacks leaves both queues intact. Static back/forward navigation commits the document, history cursor, and status together or leaves them unchanged. An offline caller-clocked timer fixture verifies that observers see the committed document and their microtasks run before the next task. | Internal timer dispatch, browser phases, asynchronous document lifecycle, BFCache, subresources and script execution. |
| Renderer streams | Snapshot diff rejects zero and duplicate IDs before invoking callbacks. Opt-in contiguous replay detects missing history before delivering deltas. An offline renderer fixture verifies recovery after installing a matching snapshot and retry after callback rejection. | The diff is quadratic and does not describe tree ordering or payloads. Atomic browser snapshot capture, cross-journal cursor identity, automatic resync, and concurrent replay remain unsupported. |
| HTML | The legacy tree builder recovers selected omitted end tags. An [opt-in parser](html-native-dom-profile.md) builds only the native DOM and publishes it transactionally, with explicit byte, node, and depth limits. | The default loader still uses the legacy tree. Full WHATWG insertion modes, script execution and suspension, templates, foreign content, and fragment parsing remain unsupported in the native profile. |
| DOM | Live ranges track native nodes, using UTF-16 offsets for character data and child indices for containers. Mutations update boundaries; failed edits preserve them and document destruction invalidates outstanding ranges. The same native tree supports bounded selectors and live-range text extraction. | Persistent numeric IDs, full selector grammar, native serialization, observation, selection, and Range editing operations. See the [native DOM contract](mulib-dom.md). |
| WebIDL | The compiler emits executable MicroJS bindings for plain, non-overloaded namespace operations with required strict primitive arguments. The generated bindings were compiled and exercised against trusted native fixtures. | Namespace objects, DOM wrappers, brand checks, GC integration, exposure checks, and full WebIDL conversion. See [binding status](webidl-integration-status.md). |
| Network | Fetch validates final response metadata before publication. The one-shot UTF-8 decoder replaces malformed prefixes without skipping the next unrelated byte. | Authenticated per-hop transport, actual-peer authorization, redirect/CORS and credential enforcement, bounded streaming/decompression. See [network verification](browser-network-status.md). |
| Security | Origin and realm comparisons reject noncanonical records; invalid permission state and quota metadata fail closed. An uncertain broker commit prevents beginning another job on that broker. | Opaque and partitioned origins, process isolation, preemptive enforcement and durable reconciliation. See the [security ledger](browser-security-ledger.md). |

```sh
make build/test-browser-eventloop-callback-free build/test-browser-renderer-diff build/test-htmlwriter
./build/test-browser-eventloop-callback-free
./build/test-browser-renderer-diff
./build/test-htmlwriter
```

All three passed after integration. Each family also passed its focused
Clang ASan/UBSan test after integration. `tests/test_microjs_engine.c`
also passed with strict GCC and Clang and with Clang ASan/UBSan. To rerun the
strict MicroJS suite, substitute `gcc` or `clang` for `cc`:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels \
    tests/test_microjs_engine.c -lm -o build/test_microjs_engine
./build/test_microjs_engine
```

These changes add no kernel dependencies and leave existing signatures unchanged.
The renderer replay fixture can be rerun with
`make build/test-browser-renderer-resync && ./build/test-browser-renderer-resync`.
The static browser host fixture can be rerun with
`make build/test-browser-engine-timed-traversal && ./build/test-browser-engine-timed-traversal`.
The mixed checkpoint fixture, `tests/test-browser-eventloop-microjs.c`, passed
under Clang ASan/UBSan after integration. It verifies that the renderer sees
completed trusted JS and native microtasks before the next browser task.
The HTML recovery fixture is `tests/test_html_tree_recovery.c`. The optional
provider fixture, `tests/test_web_optional_providers.c`, verifies denial for
unbound adapters; it does not confer browser-level authorization or enable
devices.

The P1 worker slice adds cancellation before startup, fragment-independent
registration lookup, rejection of ambiguous worklet names, and conservative
lock checks on malformed inputs. `tests/test_web_workers.c` passed offline;
these helpers do not start worker threads, run scripts, or intercept Fetch.
The P1 CSS slice adds per-property `!important`, simple-selector specificity,
and source-order ranking to the bounded HTML-tree cascade. It does not add
native DOM matching, full computed values or renderer publication; see the
[CSS integration status](css-integration-status.md).

The P1 Web-core slice adds a transactional, byte-budgeted streaming UTF-8
decoder with fatal and BOM options. Browser objects and stream integration are
still absent. See [Web-core staging status](web-core-staging.md).
The P1 storage slice adds in-memory IndexedDB batches, Web Storage limits,
and exact-GET CacheStorage operations. It does not add persistence,
browser-managed partitioning or filesystem confinement. See
[offline storage status](browser-storage-offline.md).
The P1 [Wask interpreter](mulib-wask.md) validates direct in-module calls,
including forward references, and executes them with shared fuel and bounded
nonrecursive frames. It does not add imports, memory, or JS bindings.

## Remaining build and acceptance gates

The baseline GCC and Clang CMake builds stopped at `tests/test_prettybridge.c`
because the On1x PrettyPrint runtime was removed. Cleanup now retires that
fixture's build wiring while retaining the native PrettyPrint checks. Both
integrated default builds reached 100%. The first full CTest run with each
compiler passed 808/814 tests. The Termscript/CapASM test was one of six
failures because its hard-coded temporary volume was full. It now honors
`TMPDIR` and passed on both compilers in a targeted rerun, which also
verified the final DOM changes. The other five failures were dependency
strata, MuLL inventory, manual inventory, generated ArchML profiles, and
embedded unit profiles. After the three-worker follow-up, the integrated
GCC build passed and full CTest passed **815/816** with `TMPDIR` set to the
workspace's temporary directory. Only `mulib_manual_inventory` failed: all
224 staging headers remain unregistered. The MuLL inventory now includes them
for discovery without adding legacy callable exports. The full Clang suite
has not been rerun on this final revision.

Dependency strata, ArchML generation, and embedded unit checks now pass.
Do not treat a regeneration as approval of every staged header:
registration requires validating the dependency and implementation contracts
and supplying the corresponding manuals and examples.

No Test262, WPT, html5lib, official WebAssembly, or official Unicode
conformance result is established here. Untrusted script, network and device
execution remain out of scope until enforceable isolation, real peer
authorization, cancellation and resource bounds are integrated and verified.
