# Elvi document capture

Document capture is opt-in and uses the ElviScrape compiler and ElviScript
HTTPS transaction, not Lua or its IPC results. Compilation is pure:

```text
elvi example_document
host www.example.org
home https://www.example.org/article
mode document
artifacts capture
```

`mode document` requires `artifacts capture`; it rejects XPath, Lua, IPC,
and legacy search directives. An optional `cookies provide{PATH}` stage
uses the existing secure scoped loader; a `$HOME` prefix uses only an explicitly
supplied, approved home. Neither the DSL nor its result grants authority.
`mode scrape` still requires Lua, XPath, IPC, and `artifacts discard`.
The Lua/IPC runner does not hand out the source document. Passing a discard
profile to document capture is rejected before any file or network access.

## Trusted native boundary

The raw C companion `capture.h`/`capture.c` consumes MuLIB declarations and
does not emit implementations. Link one implementation TU for ElviScrape,
ElviScript Transport, and their dependencies. No Lua or IPC implementation
is required for this operation.

* `on1x_elvi_capture_https(program, config, out)` delegates to
  `mulib_elvi_script_capture_https`. Borrow the unmodified compiled program,
  config and its strings, and TLS callbacks/context until return. Input
  structures and outputs must not overlap or be concurrently changed.
  The host must select `approved_host` independently of the profile and any
  request data, and `approved_cookie_path` independently of the resolved
  cookie argument. Cookie approval is not granted by the host name match.
  `config.url`, if supplied, must pass the same exact-host HTTPS gate as
  the home; otherwise the home is fetched. Public DNS syntax, no explicit
  port, no userinfo, and no fragment apply. The host backend must vet the
  actual resolved destination, independently verify the hostname and
  certificate chain, and apply finite connection and I/O deadlines.
  There is no network or TLS backend selected by default.
* Use an empty output, e.g. `{0}`. `MULIB_ELVI_CAPTURE_OK` (zero) transfers
  ownership of `body`; failures leave the output unchanged. The body is not
  C text, and embedded NUL is preserved. Call
  `mulib_elvi_script_document_destroy` exactly once for each successful
  owner, or move that owner without duplicating it. Destroy resets the
  caller's struct and is safe again on the reset struct; a copied owning
  struct is **not** a second owner. Destruction frees memory without promising
  erasure in process or kernel caches.
* `on1x_elvi_document_copy_hex(document, out, capacity)` borrows the
  document and writes into caller-owned nonoverlapping storage. It preserves
  bytes as lowercase ASCII, in `2 * body_length + 1` bytes including the
  terminator. A zero return leaves the destination unchanged. This is an
  explicit representation conversion, not UTF-8 decoding or sanitization.

The synchronous HTTPS transaction performs at most one GET. It rejects
redirects rather than following `Location`. It has no decompression,
redirect/retry handling, persistence, cache, or implicit cookie updates.
Limits are the requested positive body limit (maximum 2 MiB), 32 KiB
header bytes, 100 headers, and raw wire limit equal to the body limit plus
64 KiB. It rejects overflow rather than truncating. The first rejected
layer decides the error: for example, a 3xx message that exceeds the body
limit fails at HTTP parsing, before redirect classification.

The nonzero native result distinguishes invalid profile/configuration,
denied host/path, unusable cookie store/header, failed TLS connection or
peer check, failed TLS I/O, unacceptable HTTP framing/encoding, redirect,
limit exceeded, and allocation failure. A failure returns neither
metadata nor a partial body. Cookie import failures are grouped as
unusable credentials, including resource failure within the loader.

Success describes a complete TLS/HTTP transaction, including a final
2xx, 4xx, or 5xx status. A non-2xx response is not a transport failure;
the host/application decides which statuses and media types to accept.
The metadata identifies the requested and effective URL (exactly equal),
HTTP/1.1 framing, raw bounded ASCII `Content-Type` (255 bytes, empty
if absent; duplicates rejected), TLS version from the authenticated
session, certificate and hostname verification, and ALPN (empty if absent).
No arbitrary response headers, trailers, request cookies, reason phrases,
fingerprints or `Location` values are copied. URLs, content type and body
can nonetheless contain private data; the caller controls access and
retention. Success is reported only after authenticated EOF, full framing
validation, and session close. No timestamps or duration are measured;
`close_delimited` denotes HTTP framing, not timing or a daemon/subprocess
lifecycle. The application can measure around this synchronous operation
using its own clock and policy.

The transport requires TLS 1.2 or 1.3 and HTTP/1.1 or absent ALPN, with
successful peer inspection before request bytes can be written. The peer
callback returns **negotiated** facts, not advertised capabilities. All
callbacks are trusted and synchronous; this interface cannot interrupt a
host callback that ignores its deadline or detects TCP EOF instead of
authenticated TLS shutdown. Callback dishonesty, live DNS/TLS correctness,
and real network health are not established by the fixture.

## On1x value boundary

`document.on1x` exposes `BasisElviDocumentMetadata`, `BasisElviDocument`,
`basis_elvi_document_from_hex` (typed `Result`, `Err(1)` on invalid shape)
and `basis_elvi_document_valid`. The host copies the bounded metadata and
hex body into ordinary managed values; no native address, callback, or
native owner is carried across this boundary. The metadata validator
checks shape, not provenance, hostname trust, or DNS. An explicitly
installed transport operation must supply trustworthy metadata and lifecycle
results; a record literal alone neither performs I/O nor proves TLS.

`capture.on1x` exposes
`basis_elvi_capture(program, approved_host, max_body_bytes, fetch)`.
It revalidates the compiled source, requires document mode and an exact
independently supplied host, and calls the supplied synchronous closure
once, with a typed `BasisElviCaptureRequest` for the profile home. It
rejects mismatched response URLs, oversize bodies and invalid snapshots.
This portable typed lane rejects profiles declaring a cookie stage without
calling the transport; use the explicit native companion for an approved
cookie file. A capture operation is not automatically a live backend.

These core document and capture modules make no raw On1x foreign
declaration for transport. The capture operation uses the existing Elvi
profile inspection API. Exotract can inspect the C declarations, but
declarations alone do not prove
ownership, allocation, callbacks, or effects. No ExoML admission or
Chisel/Wetstone-backed cross-language ownership claim is made here. A
future direct binding would require reviewed obligations, independent
verification with the actual link toolchain, and an admitted ownership
model. Existing script IPC may still carry deliberately extracted
logical results, not a retained artifact.

## Qualification

`tests/test-elvi-script-transport.c` exercises real native HTTP parsing,
Elvi policy, TLS callback order and cleanup with an offline TLS fixture.
It checks binary NUL preservation, exact request identity, allowed and
rejected TLS facts, malformed and oversized responses, credential
authorization, and injected allocation failures. The On1x fixtures
`on1x/stdlib/tests/elvi_capture.on1x` and `elvi_capture_operation.on1x`
exercise value validation and the linked Elvi preflight with a trusted
fixture closure; they require an `on1xc` supporting nominal string fields.
