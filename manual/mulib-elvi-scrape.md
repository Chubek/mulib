# MuLIB ElviScrape — `.elvs` search-provider DSL

ElviScrape is a Surfraw-inspired, **C11 header-only** provider interpreter. Surfraw's
elvi primarily dispatch a query URL to a browser; this module also extracts a
bounded list of `{title, url}` records from supplied HTML or JSON search
responses. It neither embeds Surfraw/SearXNG nor executes provider code.

## What this takes from SearXNG

Local SearXNG revision `b867c56a5668966ef4be8097f1b559bdc2841743` has 353
default settings entries referencing 228 distinct engine modules, out of
253 adapter files. These are inventory counts, not live availability counts.
In `searx/engines/__init__.py` and `searx/search/processors/online.py`, each
configured entry supplies request and response logic while the processor
owns transport. `searx/engines/xpath.py` selects result containers and reads
fields from each, while `searx/engines/json_engine.py` maps fields over
structured responses. ElviScrape independently applies that split:
compile a description, render the request, fetch under policy, and extract
fields together from each anchor or JSON row. It does not copy upstream
Python, use its recursive JSON query language, or implement its scheduling,
ranking, provider traits, or per-engine scripts.

## Paths

- `include/mulib-elvi-scrape.h` — public API and implementation switch.
- [`library/elvis/`](../library/elvis/README.md) — shipped profiles with
  template and response-field provenance.
- `tests/test-elvi-scrape.c` — deterministic offline behavior tests.
- `tests/test-elvi-scrape-providers.c` — provider URL and extraction tests.
- `tests/test-elvi-scrape-https.c` — independent authenticated-session/HTTP tests.
- `tests/test-elvi-scrape-alloc.c` — exhaustive single-allocation failure injection.
- `examples/elvi-scrape.c` — offline parse / URL construction example.

## Dependencies (MuLIB only)

The module uses `fetch`, `url`, `https`, `json`, `utf8` and the internal webscan scanner.
The complete one-TU implementation recipe is: `parsec`, `strbuf`, `buffer`,
`inet4`, `inet6`, `uri`, `utf8`, `json`, `bounded-text`, `browser-origin`, `fetch-policy`,
`fetch`, `url`, `http`, `webclient`, `tsl`, `https`, `elvi-scrape`. Define all
corresponding `MULIB_IMPL_*` macros before including `mulib-elvi-scrape.h` in
exactly one C11 translation unit. Other translation units include declarations
only. No implementation switches are enabled implicitly, including the optional
owned `MULIB_IMPL_PARSER` API.
On platforms with a separate C math library, link `-lm`: the complete one-TU
dependency closure includes `bounded-text`'s decimal conversion helpers.

Offline compilation and extraction never open a connection. The HTTPS adapter
composes MuLIB HTTPS, HTTP and TSL kernels with an application-supplied blocking
TLS connector and authenticated-peer inspector; it adds no external dependency.

## `.elvs` grammar

One directive per line. `#` begins comments outside quoted strings. Values may
be unquoted single tokens or double-quoted strings; in quotes `\\` and `\"`
are valid escapes. Directive names are lowercase and case-sensitive. `elvi`,
`host`, `home`, and `search` are always required. Only `exclude_host` can
repeat, up to eight times. HTML is the default; prior HTML profiles still work.

```text
elvi NAME
host EXACT_HOSTNAME
home https://EXACT_HOSTNAME/
search "https://EXACT_HOSTNAME/search?q={query}&count={limit}"
format html            # optional; default
anchor child h3        # title H3 nested within result <a href=...>
# or: anchor parent h2 # result <a href=...> nested within H2
# or: anchor any       # any anchor with text (legacy `anchor any a` also works)
anchor_class hit        # optional class token on the <a>
rewrite direct         # direct HTTPS anchors
# or: rewrite google   # also recognizes Google's /url?url=... or /url?q=...
# or: rewrite relative # also recognizes root-relative /path on EXACT_HOSTNAME
exclude_host bing.com  # block the given host and subdomains from results
max_results 30         # 1..64; default 20
```

For JSON, replace `format html` and the HTML anchor directives with:

```text
format json
results /message/items # one array, or / to select a root array
title /title/0         # string in each item
url /URL               # string in the same item
```

JSON selectors are slash-prefixed paths up to 255 bytes, using RFC 6901
`~0` and `~1` escapes and canonical decimal array indices. `results /` is an
ElviScrape special case for the root array. `title` and `url` are evaluated
relative to each row. They do not search descendants or zip unrelated arrays.
Missing/non-string fields are skipped; a missing/non-array result collection
or invalid JSON fails. Only `direct` and `relative` rewrites are valid for JSON.

For a nonempty query, the `search` template must include `{query}`, which
expands to URL-component-encoded query bytes using `mulib_url_encode` (spaces
become `%20`). The optional `{limit}` expands to the requested result maximum.
The placeholders can occur in the path or query, but not in the authority or
inside a percent escape. No other placeholders or expressions are supported.
An empty query (`""`) selects `home`, like the no-terms branch of Surfraw's
Google and Bing elvi. It does not fall back to `home` on request failure.
Both URLs must use `https://` and a literal request hostname matching `host`
(ASCII case-insensitive). The caller supplies one query string; the module
does not parse Surfraw shell options or quote/split query words.
Host exclusions are enforced on **result URLs**, never just their display text.
Only HTTPS result links are emitted. The `relative` rewrite resolves a path
starting with exactly one slash against the configured request host (never an
HTML base tag or an origin in the response). Scheme-relative, path-relative,
and non-HTTPS links are not accepted. `google` decodes recognized wrappers.
Google wrappers accept one nonempty `q=` **or** `url=` target before a fragment;
repeated/ambiguous targets and malformed escapes anywhere in the wrapper are
rejected. Invalid numeric character references in URL attributes are rejected,
not silently passed through. Request `home` and rendered `search` URLs must not
contain fragments: a fragment is not sent to the provider as a query.

The DSL is **declarative and non-Turing-complete**: no commands, process execution,
Lua, embedded JavaScript, or arbitrary memory access. A provider file can alter
its configured remote host, however, so load provider files only from locations
you trust. The fetch wrapper intersects the caller's policy with HTTPS-only, no private
hosts, max 2 MiB response data and max 8191 URL bytes (plus NUL). It preserves
an explicit `allow_https = 0`, and smaller caller limits. Zero limits retain
MuLIB default semantics, bounded by these module limits. Host checks reject
userinfo, ports, IPv6 literals, malformed DNS labels, private/ambiguous numeric
hosts, and local names covered by MuLIB fetch policy. All percent escapes must
be valid; literal whitespace/backslashes and decoded NUL/control/backslash bytes
are rejected. The policy is lexical: DNS resolution and rebinding protection
remain the transport's responsibility.

## Public API

```c
int mulib_elvi_scrape_compile(const char *source, size_t length,
    mulib_elvi_scrape_program_t *out, mulib_elvi_scrape_error_t *error);
int mulib_elvi_scrape_load(const char *path,
    mulib_elvi_scrape_program_t *out, mulib_elvi_scrape_error_t *error);
int mulib_elvi_scrape_url(const mulib_elvi_scrape_program_t *program,
    const char *query, unsigned limit, char *out, size_t capacity);
int mulib_elvi_scrape_parse(const mulib_elvi_scrape_program_t *program,
    const char *body, size_t length,
    mulib_elvi_scrape_result_t *results, size_t capacity, size_t *count);
int mulib_elvi_scrape_search(const mulib_elvi_scrape_program_t *program,
    const mulib_fetch_client_t *client, const char *query, unsigned limit,
    mulib_elvi_scrape_result_t *results, size_t capacity,
    size_t *count, unsigned *http_status);
```

All functions return 1 on success and 0 on failure. A successful `parse` or
`search` may produce **zero results**. `compile` is transactional: an invalid
file leaves its destination unchanged and optionally reports a 1-based line
number (`0` for errors not associated with a source line). The caller owns
result buffers. No global mutable state is used. `url` leaves output unchanged on failure;
`parse` leaves results/count unchanged on failure. `search` changes results/count
only on success, but reports HTTP status after a successful fetch even when that
status causes rejection. Inputs and output buffers must not overlap. Programs
must come from successful compilation and remain unmodified during calls.
Rebuild consumers against the current header: the caller-owned program struct
has grown to store the new selectors.
Queries are bounded to 2048 bytes, and `limit` must be in `1..max_results` even
for an empty query. `search` fetches the same URL that `url` returns and then
parses the response; a home page may have no matching results.
Titles are clipped at UTF-8 code point boundaries. HTML titles skip invalid
UTF-8 bytes and decode the five XML entities, `nbsp`, and valid numeric
references; this is not a full HTML entity table. JSON titles are decoded JSON
strings, not markup, and strings containing a NUL are skipped. Literal and
decoded NBSP in titles are collapsed like ASCII whitespace. Duplicate results
are removed by exact destination URL bytes, without reinterpreting path escapes
or changing query semantics.

HTML extraction stages at most `min(capacity, max_results)` records (never more than
64) and an HTML copy before committing caller outputs. The scanner is iterative
and limited to `16 * length + 1024` work steps; exhausted work or allocation
failure returns 0 without committing partial results. Scratch result storage is
at most 64 times `sizeof(mulib_elvi_scrape_result_t)`, in addition to the bounded
HTML copy and fixed-size URL/title buffers. A parent `td` or `th` selector also
needs at most `length / 7 + 1` bytes of cell nesting state in the same allocation
as the HTML copy. For HTML, zero capacity succeeds with zero results after
input/program validation. JSON extraction validates and builds an owned tree
using [mulib-json](mulib-json.md), accounting for its nesting, parse and
object-key work limits. JSON still validates the document and array when
capacity is zero; rows are emitted only after the tree and result array have
passed validation.

## Example

```c
mulib_elvi_scrape_program_t program;
mulib_elvi_scrape_error_t error;
char request_url[8192];
if (!mulib_elvi_scrape_load("library/elvis/Google.elvs", &program, &error)) {
    /* report error.line and error.message */
}
if (!mulib_elvi_scrape_url(&program, "C11 compiler", 10,
                           request_url, sizeof request_url)) {
    /* invalid/too-long query or URL */
}
/* Or use a configured mulib_fetch_client_t and mulib_elvi_scrape_search. */
```

Build and run the standalone demo from the repository root. Without a saved
page it prints only the generated URL; neither mode performs network I/O.

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude examples/elvi-scrape.c -lm -o elvi-demo
./elvi-demo library/elvis/Google.elvs "C11 compiler"
./elvi-demo library/elvis/Google.elvs ""
./elvi-demo library/elvis/Google.elvs "C11 compiler" tests/fixtures/elvi-scrape/layout.html
```

## Verified HTTPS transport

Set `client.transport = mulib_elvi_scrape_https_transport` and `client.ctx` to a
live `mulib_elvi_scrape_https_t`. Explicitly set `client.policy.allow_https = 1`.
The adapter borrows `connect`, `peer`, and `context` for the synchronous call.
`connect` follows the `mulib_https_connect_fn` contract and initializes an owning
TSL session. The adapter runs the verified handshake before `peer` inspects it,
and before any HTTP bytes are sent. `peer` fills `mulib_elvi_scrape_peer_t` from
the **negotiated, authenticated session**, reporting the original hostname,
certificate and hostname verification, TLS 1.2 (`0x0303`) or 1.3 (`0x0304`), and
ALPN `http/1.1` (or empty for HTTP/1.1 without ALPN). Old/unknown protocols,
wrong hostnames, unverified peers and HTTP/2 are rejected. Every created session
is closed on success or failure, including partial connection failures.

The callbacks are trusted application code: the adapter cannot detect invented
verification reports. They must enforce chain and hostname validation, bounded
connection waits, public resolved addresses, and authenticated TLS EOF. MuLIB's
native `mulib-tls` is an available TLS 1.3 connector; its implementation and
additional dependencies belong to the application. Existing generic fetch
transports remain supported and must provide the same security guarantees.
The adapter returns HTTP responses without following `Location`; search accepts
only status 200 and a media type matching the profile (case-insensitive, optional
parameters). HTTP redirects, 429 and 503 never trigger follow-up requests or
retries. Rate scheduling and retry decisions belong to the application.
The adapter rejects duplicate `Content-Type` fields, duplicate `Content-Encoding`
fields, and encodings other than `identity`; it does not implement decompression.
Body limits include bytes already in the initialized caller buffer. Failed
framing, metadata, capacity or allocation checks preserve body/status/type.

## Important limitations

- Search providers' HTML layouts are not stable APIs. Selectors are
  heuristic. Results may be empty if the provider shows a consent page,
  CAPTCHA, JavaScript-only interface, or modified markup.
- `anchor child h3` and `anchor parent h2` are simple structural patterns,
  **not** full CSS or XPath. `anchor child` captures only text inside its
  matching descendant; `anchor parent` captures anchor text inside its
  configured ancestor. Tags and attribute names match independently of ASCII
  case. `href` can be quoted or unquoted; only its first occurrence counts.
  An optional `anchor_class` matches a literal, case-sensitive, ASCII
  whitespace-delimited token on the anchor itself, using its first `class`
  attribute. Class values are not entity-decoded.
- This version extracts title and destination URL. It does not extract
  snippets, search ranking metadata, or pagination offsets.
- Malformed HTML is best-effort; the shared MuLIB webscan scanner is intentionally
  not a complete HTML5 parser. Heading changes/closures terminate stale heading
  context. In `anchor parent td` or `anchor parent th`, cell and row boundaries
  can also end the context even when a closing cell tag is omitted. Comments/
  declarations, nested templates, script/style, RCDATA and
  fallback containers (`textarea`, `title`, `xmp`, `iframe`, `noembed`, `noframes`,
  `noscript`) cannot introduce results or title markup. `plaintext` ends markup
  scanning. This conservative subset does not implement CSS visibility or the
  complete HTML5 tree-construction/namespace rules.
- HTML requires `text/html` or `application/xhtml+xml`; JSON requires
  `application/json`. A JSON profile does not accept script or JSONP media
  types. The supplied generic transport must
  not follow redirects implicitly; the provided HTTPS adapter never does.
- The module does not bypass bot detection, paywalls, CAPTCHAs, rate limits,
  or search-provider restrictions. Use responsible request rates and comply
  with applicable site terms and laws.
- Result links are returned as untrusted strings. Validate again before
  following them. A result extraction policy is not a defense against DNS
  rebinding or an insecure caller-supplied TLS transport.

## Verification and On1x registration

Tests use actual URL/fetch/policy/JSON/HTTPS/TSL kernels with offline HTML, JSON
and TLS session fixtures. Optional live proxy smoke results are recorded in
the [provider inventory](../library/elvis/README.md). From the repository root:

```sh
make build/test-elvi-scrape build/test-elvi-scrape-https build/test-elvi-scrape-alloc \
     build/test-elvi-scrape-providers
./build/test-elvi-scrape
./build/test-elvi-scrape-https
./build/test-elvi-scrape-alloc
./build/test-elvi-scrape-providers
```

CTest supplies the source directory when running from an isolated work directory.
Enable `MULIB_SANITIZERS` for ASan/UBSan. With Clang and `MULIB_BUILD_FUZZER`,
`mulib_fuzz_elvi_scrape` fuzzes the DSL compiler, HTML/JSON extractor and URL/redirect
processing. Seed it with the providers from `library/elvis` and
`tests/fixtures/elvi-scrape` files.

`MANIFEST.yaml` and both On1x catalogs register `elvi-scrape`. The ExoML v2
sidecar records all six exported functions, caller-owned outputs, transactional
failure behavior, and unknown effects of indirect transport callbacks. Its
header/manual hashes and one-TU dependency recipe are pinned, together with the
shared scanner, offline providers/HTML fixtures, allocator tests and fuzz harness
as source evidence. Annotations remain
**unreviewed**, and neither extraction nor successful tests imply safe On1x ABI
lowering. Exotract requires the six symbols from the generated archive.
[`on1x/stdlib/mulib/catalog.json`](../on1x/stdlib/mulib/catalog.json) registers
`elvis` for the profiles in `library/elvis/`. Explicit fixture selection
preserves the same source-pinned recipe as kernel selection (including leaving
optional `MULIB_IMPL_PARSER` off).
The generated On1x acceptance client compiles the embedded provider data offline
through a test-only scalar C adapter; no general safe FFI is claimed.

Composable CSS selectors, provider pagination offsets and rate scheduling remain
future work. The offline fixtures ensure pagination links are not mistaken for
results and no automatic pagination or retries occur. Keep the module together
until reusable compiler/selector/runner APIs have stable tests.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-elvi-scrape.h`](../include/mulib-elvi-scrape.h) |
| Implementation switch | `MULIB_IMPL_ELVI_SCRAPE` |
| Repository-local includes | [`include/detail/mulib-webscan.h`](../include/detail/mulib-webscan.h), [mulib-fetch](mulib-fetch.md), [mulib-https](mulib-https.md), [mulib-json](mulib-json.md), [mulib-url](mulib-url.md) |
| Complete example | [`examples/elvi-scrape.c`](../examples/elvi-scrape.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/elvi-scrape.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
