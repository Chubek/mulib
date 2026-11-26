# ElviScript HTTPS transport

`kernels/mulib-elvi-script-transport.h` provides the synchronous HTTPS
implementation of `mulib_elvi_script_fetch_fn`. Define
`MULIB_IMPL_ELVI_SCRIPT_TRANSPORT` in one C translation unit and link the
ElviScrape, HTTPS, TSL, WebClient, HTTP and other transitive MuLIB
implementations. The transport takes an unmodified compiled profile, a
trusted `mulib_elvi_scrape_https_t` backend, and optionally a cookie store.
All three inputs are borrowed. No network or cookie-file access occurs
during DSL compilation.

## Authorize the input before loading it

For a `cookies provide{path}` stage, the host resolves the argument using an
explicitly approved home directory; it must independently select the absolute
path the profile may read. Only then call:

```c
mulib_elvi_script_cookies_t *cookies =
    mulib_elvi_script_cookies_load(program, resolved_path, approved_path);
```

Do not take `approved_path` from the profile, Lua, or the path resolver's
output. The loader requires an exact string match; unmatched, relative,
dot-component, empty-component, and symlink paths are rejected. It walks
directories by descriptor and only trusts root- or effective-user-owned
ancestors that are not group/other-writable, unless sticky. The final file
must be a nonempty regular file owned by the effective user, with one hard
link and no group/other permissions (for example, mode `0600`). The read is
bounded separately from the inode size check. The file is closed on every
path. Build the POSIX loader with `_POSIX_C_SOURCE=200809L` (or
`_GNU_SOURCE`) before system headers; unsupported builds return `NULL`.

Alternatively, a host that already possesses authorized bytes may use
`mulib_elvi_script_cookies_parse(program, bytes, length)`. This performs no
file access. Both imports return `NULL` on failure and an owned, empty store
for `[]`. Do not treat an import error as anonymous access. Destroy the
store with `mulib_elvi_script_cookies_destroy`; `NULL` is accepted. The store
and temporary direct load/header buffers are wiped before release. This is
not a claim that lower HTTP/TLS layers, operating-system caches, or host
process memory never hold copies.

## Cookie JSON v1

The strict JSON format is an array of flat objects, for example:

```json
[
  {
    "name": "session",
    "value": "fictional-example",
    "domain": "www.example.org",
    "path": "/app",
    "secure": true,
    "sameSite": "Strict",
    "hostOnly": true,
    "expires": 2000000000
  }
]
```

- `name`, `value`, `domain`, `path`, `secure`, and `sameSite` are required.
  `hostOnly` defaults to `true`. An omitted or `null` `expires` means a
  session cookie for the lifetime of the store. Other expiry values must be
  decimal integer UTC Unix seconds, with no fraction or exponent and an
  absolute value at most `253402300799`. An expiry at or before the current
  time does not match. If the clock cannot be read when needed, the request
  fails closed.
- Names are nonempty ASCII HTTP tokens; values are ASCII cookie octets
  (empty is allowed), not pre-quoted header text. Paths are printable ASCII,
  start with `/`, and exclude spaces, `?`, `#`, `;`, and `\`. Matching is
  case-sensitive on the URI's raw path, ignoring the query, with the HTTP
  cookie path-prefix/slash boundary rule. An empty URI path means `/`. As
  with HTTP cookies generally, path matching is not an authorization boundary
  for the server's application or for different pages on the same origin.
- The cookie store is bound to the one HTTPS host from the approved profile
  home. Host-only domains must match it exactly (case-insensitively); domain
  cookies may specify the exact host or a qualified parent suffix at a DNS
  label boundary, with an optional leading dot. Parent domains never extend
  the store to siblings or child hosts; there is no public-suffix database.
- Only `Secure` cookies are sent. `sameSite` must be one of `Strict`, `Lax`
  or `None`; `None` requires `Secure`. `__Secure-` requires `Secure`;
  `__Host-` requires `Secure`, host-only scope and `/`. The transport models
  the profile's HTTPS home as its fixed first-party context. Every request
  targets that precise site, so all three SameSite modes allow those
  requests; there is no cross-site/embedded browser context or redirect
  exception.
- Unknown or duplicate fields, invalid types, unrelated domains, and
  duplicate cookie identities (name, normalized domain, path) reject the
  whole import. This is not a general browser export format. There are at
  most 64 KiB of JSON and 64 cookies, with per-field limits of 128 bytes for
  the name, 2048 for the value, 253 for the domain (including any leading
  dot) and 1024 for the path. Eligible cookies are ordered by longest path,
  then input order. If the `Cookie` value would exceed 8192 bytes the
  transaction fails before connecting rather than sending a partial header.

## Run a request

Create a `mulib_elvi_script_transport_t` from the profile, verified HTTPS
backend and optional cookie store, then pass it as the fetch callback context
to `mulib_elvi_script_fetch_https`. The URL must pass the Elvi profile's
MuLIB fetch policy: absolute HTTPS, exact host, default port, no userinfo or
fragment. The adapter checks the TLS handshake, certificate and hostname
verification, matching peer hostname, TLS 1.2 or 1.3, and HTTP/1.1 or absent
ALPN before letting the HTTPS layer write a request. Use a trusted backend
that verifies DNS results/destination addresses per host policy, enforces a
finite connection and I/O deadline, and reports EOF only for authenticated
TLS shutdown. The URL policy alone does not resolve or vet DNS results.

The adapter makes one GET and closes its TLS session on every path. It
rejects all 3xx responses without following `Location`, and does not retry,
decompress, process `Set-Cookie`, or write an artifact. The HTTP parser
bounds the body at 2 MiB, headers at 32 KiB and 100 fields, and the raw
response at 2 MiB + 64 KiB. Duplicate `Content-Type`, duplicate
`Content-Encoding`, and non-identity encodings are rejected. On success the
typed response contains the real status (including non-redirect errors), HTTP
message and exact effective URL. The caller determines acceptable status and
media type, and frees the message with `mulib_http_message_free`. The output
is unchanged on failure. The transport writes no credentials or responses
to logs or artifact files. Do not persist the body or response headers
without a separately designed and authorized retention policy.

## Capture a document

`mulib_elvi_script_capture_https` is a separate host entry point for compiled
`mode document` / `artifacts capture` profiles. It never exposes bodies from
`mode scrape` / `artifacts discard`, and does not run Lua, XPath or IPC.
Supply a trusted verified TLS backend, a positive body limit no larger than
2 MiB, and an independently authorized exact canonical host. Use the
profile home or an explicit same-host URL for one GET. If the profile
declares cookies, it must also match the independently approved absolute
cookie path before the secure loader opens it. Do not fall back to an
anonymous request on a cookie error.

After the HTTPS session closes and framing is validated, success transfers a
binary, length-delimited body (including any NUL bytes) and bounded
metadata: final status, identical requested/effective URL, HTTP protocol and
framing, `Content-Type`, TLS version, verification state, and actual ALPN
(or absence). Other headers, credentials, and timestamps are not returned.
There is no redirect, retry, or truncation; any failure leaves the output
unchanged. Use an empty output and destroy a successful owner via
`mulib_elvi_script_document_destroy`. See the
[On1x capture notes](../on1x/stdlib/elvi/capture.md) for the companion
contracts and limitations.

## Offline check

The focused test uses fixture TLS operations; it never contacts a provider:

```sh
cc -std=c11 -O2 -Wall -Wextra -Werror -pedantic -Ikernels \
  tests/test-elvi-script-transport.c -lm -o /tmp/test-elvi-script-transport
/tmp/test-elvi-script-transport
```
