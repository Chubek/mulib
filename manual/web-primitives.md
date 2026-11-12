# Web, extraction and pricing primitives

[Manual index](index.md) · [Module guide](modules.md)

These 67 granular modules compose without third-party runtime dependencies.
Their names describe focused profiles, not complete browser, WebIDL, CSS,
ECMAScript, Schema.org, forecasting or security implementations.

## Composition

| Layer | Starting points |
| --- | --- |
| Owned bytes and money | [shylock-core](mulib-shylock-core.md), [money](mulib-money.md) |
| Price records and analysis | [observations](mulib-price-observation.md), [normalization](mulib-price-normalize.md), [matching](mulib-price-match.md), [history](mulib-price-history.md), [series](mulib-price-timeseries.md), [forecasts](mulib-price-forecast.md), [confidence](mulib-price-confidence.md), [demand](mulib-price-demand.md), [monitoring](mulib-price-monitor.md), [ensembles](mulib-price-ensemble.md), [causal diagnostic](mulib-price-causal.md) |
| Retrieval and discovery | [providers](mulib-websearch-provider.md), [results](mulib-websearch-engine.md), [crawl frontier](mulib-crawler.md), [fetch policy](mulib-fetch-policy.md), [transport adapter](mulib-fetch.md), [archive](mulib-crawlarchive.md) |
| HTML and extraction | [lexical events](mulib-html-textstream.md), [tokens](mulib-html-tokenizer.md), [tree](mulib-html-treebuilder.md), [native bridge](mulib-html-dombridge.md), [queries](mulib-dom-query.md), [text](mulib-readability.md), [JSON-LD spans](mulib-html-jsonld.md), [offers](mulib-schemaorg.md), [rules](mulib-webextract-rules.md), [evidence](mulib-webextract-evidence.md), [consensus](mulib-webextract-reconcile.md), [LLM requests](mulib-webextract-llm.md) |
| Script/DOM substrates | [mutation journal](mulib-dom-mutation.md), [offset ranges](mulib-dom-range.md), [WebIDL handles](mulib-webidl-bindgen.md), [conversions](mulib-webidl-runtime.md), [jobs/promises](mulib-microjs-promise.md), [source registry](mulib-microjs-module.md), [native dispatch](mulib-jsdom.md) |
| CSS geometry | [tokens](mulib-css-tokenizer.md), [rules](mulib-css-parser.md), [selectors](mulib-css-selector.md), [computed fields](mulib-css-computed.md), [cascade](mulib-css-cascade.md), [boxes](mulib-css-box.md), [weighted flow](mulib-css-flex.md), [uniform grid](mulib-css-grid.md), [layout callbacks](mulib-css-layoutstream.md) |
| Browser metadata/storage | [origin](mulib-browser-origin.md), [realm](mulib-browser-realm.md), [resources](mulib-browser-resource.md), [history](mulib-browser-navigation.md), [cookies](mulib-browser-cookiejar.md), [cache](mulib-browser-cache.md), [snapshots](mulib-browser-snapshot.md) |
| Browser orchestration | [fetch](mulib-browser-fetch.md), [document loading](mulib-browser-documentload.md), [static engine](mulib-browser-engine.md), [event loop](mulib-browser-eventloop.md), [limits](mulib-browser-limits.md), [permissions](mulib-browser-permissions.md), [accounting](mulib-browser-sandbox.md) |
| Event channels | [stream](mulib-browser-stream.md), [tokens](mulib-browser-tokenstream.md), [DOM](mulib-browser-domstream.md), [layout](mulib-browser-layoutstream.md), [network](mulib-browser-networkstream.md), [execution](mulib-browser-executionstream.md) |

The HTML tree is distinct from the native [DOM](mulib-dom.md), XML and existing
HTML models; do not cast between them. The DOM bridge delegates construction to
the host. Script modules supply native queues/registries/callbacks, not an
ECMAScript engine. CSS helpers do not render pages. The browser engine currently
loads one static HTML document at a time through a caller-supplied transport.

## Shared lifecycle

- Put each implementation switch in exactly one C translation unit. Dependency
  switches remain application-owned. Every module example lists its full closure.
  Declaration-only inclusion followed by implementation inclusion is supported.
- Initialize owning structs with `{0}` before first use. Configuration limits
  may then be set explicitly. Constructors that publish an owned object (`parse`,
  snapshot `make`, engine `init`) require fresh or previously released output;
  they do not implicitly destroy an existing object. Never shallow-copy owners.
- Collection insertion copies inputs unless the API explicitly documents a
  borrowed callback/context. Free/clear/destroy accepts NULL; ordinary collection
  cleanup resets fields, including configured limits. Event-loop cleanup releases
  its queues; configure the per-turn budget explicitly when reusing it.
- Input spans must designate accessible bytes; C strings must terminate. NULL
  with nonzero length is invalid. Empty-input support is specified per module.
  Offsets and lengths are bytes, not Unicode code points. Do not overlap separate
  input/output arrays unless the module explicitly supports aliasing.
- Public struct layouts support embedding, not arbitrary forged state. Keep
  capacities, lengths, pointers and ownership consistent. Borrowed collection
  views generally expire on growth, replacement, pruning or cleanup. Tree nodes
  remain valid until their tree is freed.

## Results and callbacks

Integer operations return 1 on success and 0 on failure/predicate false unless
their manual states otherwise. Pointer lookups return borrowed pointers or NULL;
counting APIs return counts, not Boolean status. Scalar and staged buffer APIs
document failure preservation explicitly. Allocation is via standard
malloc/calloc/realloc/free; zero optional limits mean unlimited only where stated.

Callbacks execute synchronously. User contexts remain caller-owned and must
outlive registration/queued work. Parser spans and event payloads are borrowed,
not retained. Callback failure cannot undo callbacks already delivered or native
side effects. Do not destroy/reinitialize active callback owners. Streams reject
recursive emit and ignore free during dispatch; microtask queues reject nested
drain and ignore free during drain. New stream subscriptions take effect on the
next event. No mutable object is internally synchronized.

## Security boundaries

Fetch policy checks URL syntax and obvious local literals/names only. A real
transport must resolve and validate every actual peer address, prevent DNS
rebinding, recheck redirects, enforce authenticated TLS and bound decompression
and streaming. URL approval alone is not SSRF protection. The transport must use
the provided buffer limits; postvalidation cannot undo an allocation already made
by a malicious or buggy callback. Browser-origin equality is not CORS or site
isolation. Cookie Domain authorization/public-suffix handling belongs to the host.

Limits/sandbox accounting and permission bits are cooperative host tools, not
an OS sandbox. FNV fingerprints are not cryptographic authentication. LLM request
wording does not guarantee resistance to prompt injection; schema, price and
source-evidence validation remain mandatory host policy. Statistical helpers
are explicit baselines/diagnostics, not calibrated models or causal proof.

## Verification

The six focused suites cover all 67 modules, including independent ownership,
malformed/empty inputs, numeric boundaries, callback cancellation/reentrancy,
moving self-appends and deterministic allocation failure at every observed
allocation site. `test_web_hostile.c` adds deterministic bounded malformed-input
sampling; this is regression evidence, not exhaustive fuzzing or certification.
`test_money_reference.py` checks full-width scaling against an independent exact
integer reference, including rounding and unrepresentable results.

```sh
make check-web-primitives
python3 tools/check.py --compile-timeout 300
cmake -S . -B build/web-check -DMULIB_SANITIZERS=ON
cmake --build build/web-check --target mulib_test_web_pricing mulib_test_web_extraction mulib_test_web_browser mulib_test_web_css mulib_test_web_alloc mulib_test_web_hostile
ctest --test-dir build/web-check -R 'mulib_test_web_(pricing|extraction|browser|css|alloc|hostile)$' --output-on-failure
```

Each `examples/modules/<name>.c` is a complete offline program. MuLL's generated
native bridge registers the same headers, types and functions; integration
fixtures live under `mudsl/tests/mull/mulib/`.
