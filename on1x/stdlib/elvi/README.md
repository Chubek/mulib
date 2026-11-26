# `std.elvi`

Compile MuLIB `.elvs` descriptions into a nominal `ElviScript`, inspect
provider specifications and stage declarations, and render or extract bounded
search results. Loading, fetching, and running scripts require explicit
trusted host operations. Importing `std.elvi` neither opens a file nor enables
networking.

```on1x
import std.elvi as Elvi;

fn inspect_example(source: string)
{
  match Elvi.basis_elvi_script_compile(source) with {
    Ok(script) => { println(script["profile"]["name"]); },
    Err(error) => { println(error["message"]); }
  };
}
```

## Profiles and operations

| API | Contract |
| --- | --- |
| `basis_elvi_script_compile`, `basis_elvi_script_compile_buffer`, `basis_elvi_script_load` | Return the validated nominal `ElviScript`. Bounded buffer compilation copies a NUL-free source snapshot after scanning for NUL and reports the physical line of any NUL byte. |
| `basis_elvi_script_provider(script)` | Revalidates the source and returns a typed provider description. `basis_elvi_provider_search` exposes the compiled template, rewrite mode, exclusions, limit, and either `ElviHtmlSelectors` or `ElviJsonSelectors`; `basis_elvi_provider_stage` projects a single typed stage. |
| `basis_elvi_compile(source)` | Validates up to 64 KiB of source. Diagnostics carry a one-based physical line (`0` for non-line-specific errors). No file or network access. |
| `basis_elvi_load(path, approved_path, read_text)` | Checks the independently approved absolute path before calling the trusted reader, passing the 64 KiB source limit. The reader authorizes the actual file object and returns text or an error; the package does not open or close it. |
| `basis_elvi_inspect(program)` | Reports name, canonical host, home URL, mode, and result limit. `format` is `Some(ElviHtml)` or `Some(ElviJson)` in search mode and `None` otherwise. |
| `basis_elvi_stage(program, kind)` | `Ok(None)` means that exact stage is absent. Present stages report their kind, decoded argument, and source line. |
| `basis_elvi_resolve_stage(stage, approved_home)` | For cookie, Lua, and IPC path stages. Resolves only a leading `$HOME` or `${HOME}` path component, with no environment lookup, filesystem canonicalization, or grant of authority. |
| `basis_elvi_render_search(program, query, limit)` | Legacy search only. Percent-encodes a query up to 2,048 bytes. Empty query selects `home`. Limit must be `1..max_results`, even for empty queries. |
| `basis_elvi_extract_search(program, body, limit)` | Legacy search only. Parses existing NUL-free HTML/JSON text up to 2 MiB. Success may contain zero hits. |
| `basis_elvi_extract_with<D>(program, document, limit, adapter)` | Polymorphic extraction checks the adapter's declared format and limit before calling it. The stock adapters use distinct `ElviHtmlInput` and `ElviJsonInput` types; neither parser falls back to the other format. |
| `basis_elvi_hit_at(results, index)` | Zero-based optional typed `{title, url}` hit. Out-of-range indices and malformed result framing give `None`. |
| `basis_elvi_search(program, query, limit, approved_host, fetch)` | Calls the explicit synchronous transport once, only after validating the program, request, and independently approved exact canonical host. Requires status 200, an unchanged effective URL, and the correct media type. |
| `basis_elvi_run_script(program, approved_host, paths, run)` | Legacy scrape only, with declared `artifacts discard`. Resolves and compares all declared path bindings before invoking a trusted synchronous runner. `ElviIpcComplete` denotes successful sender completion, not receiver acknowledgement. No raw document or headers can be returned. |

`fetch` has type `fn(string) -> Result<ElviSearchResponse, ElviError>`. It must
independently enforce egress policy, permitted DNS results and destinations,
verified HTTPS, unambiguous media type, identity content encoding, finite
deadlines, and byte limits. It must not redirect or retry. The `approved_host`
argument is not sufficient if derived from the profile itself.

`run` has type
`fn(ElviProgram, ElviScriptPaths) -> Result<ElviScriptCompletion, ElviError>`.
`ElviScriptPaths` requires explicit XPath opt-in and a `1..30000` ms
IPC deadline. Trusted code binds a connected descriptor and authorized Lua
source bytes to their approved paths; a matched path string is not permission
to open it. The native kernel bounds Lua source to 16 KiB, caps runtime work,
and exposes XPath only inside the bounded script context. It does not make
arbitrary Lua safe to run in-process. The IPC deadline does not preempt Lua,
host callbacks, or HTTPS I/O. The descriptor owner closes it after each
one-direction run or failure.

`ElviScript` contains a compiled profile snapshot and an `ElviProgram`
(immutable replayable source); use `script["program"]` with operations that
take `ElviProgram`. The snapshots have normal On1x text lifetime and no
native destruction requirement. Exported records can be constructed by
callers, so native operations recompile the source rather than trusting any
cached metadata. Search result snapshots
carry at most 64 hits and are not an authorization or durable storage format.
Neither callback is retained. Generic `Option` and `Result` are the same
nominal types as in `std.core.Pervasive`.

## Document capture is separate

Only `mode document` with `artifacts capture` can capture a response.
It cannot run a Lua, XPath, or IPC stage. There is no conversion from
`mode scrape` / `artifacts discard` into a capture. `BasisElviDocument` and
`BasisElviDocumentMetadata` are typed snapshots, not transport capabilities.
`basis_elvi_document_from_hex` validates and accepts a reversible lowercase
hex representation of exact body bytes (including embedded NUL). See
[`capture.md`](capture.md) for the injectable typed `basis_elvi_capture`
contract and [`capture.h`](capture.h) for native capture and hex-copy. The
capture body is owned by the native caller until released via
`mulib_elvi_script_document_destroy`. Raw HTTP headers and timestamps are not
automatically retained. No cookie values or raw HTTP headers are forwarded.

## Explicit C11 linkage

Use the repository compiler with nominal string field support:

```sh
on1xc --emit-c --stdlib on1x/stdlib consumer.on1x -o consumer.c
clang -std=c11 -D_POSIX_C_SOURCE=200809L -Ion1x/runtime -Ikernels \
  consumer.c on1x/runtime/on1x_runtime.c \
  on1x/stdlib/elvi/runtime.c on1x/stdlib/elvi/bridge.c \
  on1x/stdlib/elvi/kernels.c -lm -o consumer
```

`kernels.c` is an optional standalone **single** implementation translation
unit. Omit it when an embedding already supplies the Elvi kernel dependency
closure (for example the `http-probe` example's aggregate TU). Add `capture.c`
if using native capture. A native-only embedding does not need `bridge.c` or
`on1x_runtime.c`. An embedding that needs the legacy Lua/IPC runner can link
[`script.c`](script.c) with its application's complete legacy kernel TU;
do not add overlapping standalone switches. See the
[ElviScript manual](../../../manual/mulib-elvi-script.md) and its C fixture
for the legacy dependency closure.

[`runtime.h`](runtime.h) documents the native caller-owned compile, stage,
search, and trusted path loading interfaces. Application code should use its
secure file-opening policy and call `on1x_elvi_compile` with authorized bytes
when it cannot independently guarantee the filesystem object targeted by the
path. The `fopen` convenience does not itself exclude symlinks, replacement,
devices, or FIFOs. Native search accepts an injected trusted MuLIB client;
there is no built-in resolver, TLS backend, file lookup, or public-network
default. The POSIX feature macro enables the secure optional cookie loader;
that loader fails closed on unsupported platforms.

### Generated-C ABI scope

These are the only raw externs in the core package. The optional
[`std.elvi.live`](LIVE.md) module has separate host bindings:

* `on1x_elvi_compile_v1` and `on1x_elvi_inspect_v1` validate source and return
  a diagnostic or immutable reply.
* `on1x_elvi_stage_v1` takes a stage selector and returns absence, one stage
  description, or a diagnostic.
* `on1x_elvi_search_spec_v1` describes the relevant validated template,
  selectors, rewrite and exclusions for search mode only.
* `on1x_elvi_render_search_v1` and `on1x_elvi_extract_search_v1` take a validated
  query/body and integral limit; neither does I/O.
* `on1x_elvi_resolve_stage_v1` resolves a bounded stage argument as text.

All borrow NUL-free On1x text; no native pointer handles, mutable output
buffers, or callbacks cross this ABI. Result framing is bounded and
validated before On1x slices, with up to 165 KiB of native stack staging for
the worst-case wire result. Native scratch is released before allocating a
runtime snapshot. Kernel failures that have only a boolean status are not
reclassified as precise allocator or parse diagnostics. Runtime text
allocation and cumulative text-pool exhaustion use On1x's fatal error path;
they are not recoverable `ElviError` values. Text snapshots are retained by
the owning On1x runtime thread's process-lifetime text pool.

This is an explicit trusted C11 integration, **not** an admitted foreign ABI,
packet/protocol sandbox, or cross-thread runtime API. Independent ExoML,
Chisel, and Wetstone usage evidence has not been supplied.

## Offline verification

```sh
python3 -B on1x/stdlib/elvi/tests/run.py /path/to/on1xc clang
```

The fixture exercises actual emitted On1x C11 and native calls, including
positive HTML/JSON search, legacy stage resolution, document/scrape separation,
denied host and path gates, embedded NUL rejection on native input, and
allocation failure and cleanup. It performs no live network operations.
On Linux, add `--legacy` to also run the optional
native Lua/XPath adapter with injected transport and a local Unix socketpair;
the receiver sees the typed result and completion, not the source document.
