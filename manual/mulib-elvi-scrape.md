# MuLIB ElviScrape — `.elvs` search-provider DSL

ElviScrape is a Surfraw-inspired, **C11 header-only** provider interpreter. Surfraw's
elvi primarily dispatch a query URL to a browser; this module also extracts a
bounded list of `{title, url}` records from a supplied HTML search results page.
It neither embeds Surfraw nor executes shell scripts.

## Paths

- `include/mulib-elvi-scrape.h` — public API and implementation switch.
- `library/elvi-scrape/google.elvs` — Google provider.
- `library/elvi-scrape/bing.elvs` — Bing provider.
- `tests/test-elvi-scrape.c` — deterministic offline behavior tests.
- `tests/test-elvi-scrape-https.c` — independent authenticated-session/HTTP tests.
- `tests/test-elvi-scrape-alloc.c` — exhaustive single-allocation failure injection.
- `examples/elvi-scrape.c` — offline parse / URL construction example.

## Dependencies (MuLIB only)

The module uses `fetch`, `url`, `https`, `utf8` and the internal webscan scanner.
The complete one-TU implementation recipe is: `parsec`, `strbuf`, `buffer`,
`inet4`, `inet6`, `uri`, `utf8`, `shylock-core`, `browser-origin`, `fetch-policy`,
`fetch`, `url`, `http`, `webclient`, `tsl`, `https`, `elvi-scrape`. Define all
corresponding `MULIB_IMPL_*` macros before including `mulib-elvi-scrape.h` in
exactly one C11 translation unit. Other translation units include declarations
only. No implementation switches are enabled implicitly, including the optional
owned `MULIB_IMPL_PARSER` API.
On platforms with a separate C math library, link `-lm`: the complete one-TU
dependency closure includes `shylock-core`'s decimal conversion helpers.

Offline compilation and extraction never open a connection. The HTTPS adapter
composes MuLIB HTTPS, HTTP and TSL kernels with an application-supplied blocking
TLS connector and authenticated-peer inspector; it adds no external dependency.

## `.elvs` grammar (v1)

One directive per line. `#` begins comments outside quoted strings. Values may
be unquoted single tokens or double-quoted strings; in quotes `\\` and `\"`
are valid escapes. Directive names are lowercase and case-sensitive.

```text
elvi NAME
host EXACT_HOSTNAME
home https://EXACT_HOSTNAME/
search "https://EXACT_HOSTNAME/search?q={query}&count={limit}"
anchor child h3        # title H3 nested within result <a href=...>
# or: anchor parent h2 # result <a href=...> nested within H2
# or: anchor any a     # any anchor with text
rewrite direct         # direct HTTPS anchors
# or: rewrite google   # also recognizes Google's /url?url=... or /url?q=...
exclude_host bing.com  # block the given host and subdomains from results
max_results 30         # 1..64; default 20
```

`{query}` expands to URL-component-encoded query bytes using
`mulib_url_encode` (spaces become `%20`). `{limit}` expands to the user-requested
result maximum. No other variables or expression language are supported. The
provider URL must use HTTPS, and its exact request hostname must match `host`.
Host exclusions are enforced on **result URLs**, never just their display text.
Only HTTPS result links are emitted; relative links and non-HTTPS links are
ignored, except recognized Google redirect links, which are decoded to HTTPS.
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
    const char *html, size_t length,
    mulib_elvi_scrape_result_t *results, size_t capacity, size_t *count);
int mulib_elvi_scrape_search(const mulib_elvi_scrape_program_t *program,
    const mulib_fetch_client_t *client, const char *query, unsigned limit,
    mulib_elvi_scrape_result_t *results, size_t capacity,
    size_t *count, unsigned *http_status);
```

All functions return 1 on success and 0 on failure. A successful `parse` or
`search` may produce **zero results**. `compile` is transactional: an invalid
file leaves its destination unchanged and optionally reports a 1-based line
number. The caller owns result buffers. No global mutable state is used. `url` leaves output unchanged on failure;
`parse` leaves results/count unchanged on failure. `search` changes results/count
only on success, but reports HTTP status after a successful fetch even when that
status causes rejection. Inputs and output buffers must not overlap. Programs
must come from successful compilation and remain unmodified during calls.
Queries are bounded to 2048 bytes. Titles are clipped on UTF-8 boundaries;
invalid UTF-8 bytes in titles are skipped. The five XML entities, `nbsp`, and
valid numeric Unicode references are decoded; this is not a full HTML entity table.
Literal and decoded NBSP are collapsed like ASCII whitespace. Duplicate results
are removed by exact destination URL bytes, without reinterpreting path escapes
or changing query semantics.

Extraction stages at most `min(capacity, max_results)` records (never more than
64) and an HTML copy before committing caller outputs. The scanner is iterative
and limited to `16 * length + 1024` work steps; exhausted work or allocation
failure returns 0 without committing partial results. Scratch result storage is
at most 64 times `sizeof(mulib_elvi_scrape_result_t)`, in addition to the bounded
HTML copy and fixed-size URL/title buffers. Zero capacity succeeds with zero
results after input/program validation.

## Example

```c
mulib_elvi_scrape_program_t program;
mulib_elvi_scrape_error_t error;
char request_url[8192];
if (!mulib_elvi_scrape_load("library/elvi-scrape/google.elvs", &program, &error)) {
    /* report error.line and error.message */
}
if (!mulib_elvi_scrape_url(&program, "C11 compiler", 10,
                           request_url, sizeof request_url)) {
    /* invalid/too-long query or URL */
}
/* Or use a configured mulib_fetch_client_t and mulib_elvi_scrape_search. */
```

For an example that parses saved HTML without network I/O, see
`examples/elvi-scrape.c`.

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
only status 200 and an exact HTML/XHTML media type (case-insensitive, optional
parameters). HTTP redirects, 429 and 503 never trigger follow-up requests or
retries. Rate scheduling and retry decisions belong to the application.
The adapter rejects duplicate `Content-Type` fields, duplicate `Content-Encoding`
fields, and encodings other than `identity`; it does not implement decompression.
Body limits include bytes already in the initialized caller buffer. Failed
framing, metadata, capacity or allocation checks preserve body/status/type.

## Important limitations

- Google's and Bing's HTML layouts are not stable APIs. Selectors are
  heuristic. Results may be empty if the provider shows a consent page,
  CAPTCHA, JavaScript-only interface, or modified markup.
- `anchor child h3` and `anchor parent h2` are simple structural patterns,
  **not** full CSS or XPath. Other MuLIB DOM/CSS kernels could support more
  expressive selectors in a future version.
- This version extracts title and destination URL. It does not extract
  snippets, search ranking metadata, or pagination offsets.
- Malformed HTML is best-effort; the shared MuLIB webscan scanner is intentionally
  not a complete HTML5 parser. Heading changes/closures terminate stale heading
  context. Comments/declarations, nested templates, script/style, RCDATA and
  fallback containers (`textarea`, `title`, `xmp`, `iframe`, `noembed`, `noframes`,
  `noscript`) cannot introduce results or title markup. `plaintext` ends markup
  scanning. This conservative subset does not implement CSS visibility or the
  complete HTML5 tree-construction/namespace rules.
- Content-type must be `text/html` or `application/xhtml+xml`, with optional
  parameters, and HTTP status must be 200. The supplied generic transport must
  not follow redirects implicitly; the provided HTTPS adapter never does.
- The module does not bypass bot detection, paywalls, CAPTCHAs, rate limits,
  or search-provider restrictions. Use responsible request rates and comply
  with applicable site terms and laws.
- Result links are returned as untrusted strings. Validate again before
  following them. A result extraction policy is not a defense against DNS
  rebinding or an insecure caller-supplied TLS transport.

## Verification and On1x registration

Tests use actual URL/fetch/policy/HTTPS/TSL kernels with offline HTML and TLS
session fixtures, never live search engines. From the repository root:

```sh
make build/test-elvi-scrape build/test-elvi-scrape-https build/test-elvi-scrape-alloc
./build/test-elvi-scrape
./build/test-elvi-scrape-https
./build/test-elvi-scrape-alloc
```

CTest supplies the source directory when running from an isolated work directory.
Enable `MULIB_SANITIZERS` for ASan/UBSan. With Clang and `MULIB_BUILD_FUZZER`,
`mulib_fuzz_elvi_scrape` fuzzes the DSL compiler, HTML extractor and URL/redirect
processing. Seed it with the providers and `tests/fixtures/elvi-scrape` files.

`MANIFEST.yaml` and both On1x catalogs register `elvi-scrape`. The ExoML v2
sidecar records all six exported functions, caller-owned outputs, transactional
failure behavior, and unknown effects of indirect transport callbacks. Its
header/manual hashes and one-TU dependency recipe are pinned, together with the
shared scanner, offline providers/HTML fixtures, allocator tests and fuzz harness
as source evidence. Annotations remain
**unreviewed**, and neither extraction nor successful tests imply safe On1x ABI
lowering. Exotract requires the six symbols from the generated archive.
`on1x/std/mulib/catalog.json` registers both `.elvs` files as the `elvi-scrape`
fixture group. Explicit fixture selection preserves the same source-pinned
recipe as kernel selection (including leaving optional `MULIB_IMPL_PARSER` off).
The generated On1x acceptance client compiles the embedded provider data offline
through a test-only scalar C adapter; no general safe FFI is claimed.

Composable CSS selectors, provider pagination offsets and rate scheduling remain
future work. The offline fixtures ensure pagination links are not mistaken for
results and no automatic pagination or retries occur. Keep the module together
until reusable compiler/selector/runner APIs have stable tests.
