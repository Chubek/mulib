# Storage and server extensions

These 46 modules implement focused C11 APIs, not every feature suggested by their
names. The public headers specify parameters and return values. All implementation
switches for a dependency closure must be defined **before the first include** in
exactly one implementation translation unit. No header enables a dependency's
implementation. See [dependencies](#direct-dependencies) for direct edges and the standalone
`examples/modules/` programs for complete switch sets.

## Storage and serialization

`dbschema` borrows a named, flat field array. Records must be JSON objects with
no unknown fields. Required/nullable are independent; nested arrays/objects have
no recursive constraints. `row-rdbms` composes this with `documentdb`, which stores
copied JSON text in the existing sorted memtable. Binary keys are length-aware;
row ids are ordered big-endian uint64 values. `objectdb` stores copied opaque bytes
with application-assigned nonzero type tags. `graphdb` stores directed
source/label/target triples, scanning outgoing edges in label/target order.
These stores use the memtable's sequence ordering and tombstones: older writes
are ignored, equal sequences replace. Writes return the memtable result codes
(**zero is success**). Gets return independent owned JSON values or borrowed
object bytes, as documented. They are not persistent/transactional SQL engines.

`column-rdbms` owns a dense column-major finite-double matrix; row append is
failure-atomic, including when input aliases a current column. `embeddingsdb`
owns finite-double vectors and performs exact O(n*d) Euclidean nearest-neighbor
search, with smallest-id tie breaking. Distances can overflow to infinity.
Neither supplies approximate indexes or a query language.

`bson`, `cbor` and `messagepack` share the existing owned JSON value model, avoiding
competing trees. Decode is exact (trailing input fails), bounded to 16 MiB and
64 container levels, and rejects duplicate keys and invalid UTF-8. Integer
decoding is limited to [-9007199254740991,9007199254740991], preventing silent
precision loss. Number encoders emit IEEE binary64. Non-finite values are not
representable. CBOR does not support tags, byte strings, indefinite lengths or
half floats. MessagePack excludes bin/ext types. BSON requires an object root,
excludes embedded NUL keys, and supports only double/string/document/array/bool/
null plus int32/int64 decoding. These are interoperable **subsets**, not lossless
converters for arbitrary documents of those formats.

`dbserializer` and historical `dbdesializer` dispatch JSON/BSON/MessagePack/CBOR;
all codec implementations must be linked when using either dispatcher.
`protobuf` instead exposes schema-free fields (wire types 0,1,2,5), borrowed byte
payloads and ZigZag helpers. Groups and generated message semantics are absent.

## IDL and native output

`muidl`, `webidl` and `wit` generate source from `dbschema`; these generators do
**not** parse IDL or generate bindings. Separately, [WebIDL](mulib-webidl.md) also
compiles source to S-expression CST/AST, CodeYAML and C99 declarations.
MuIDL's small record grammar is defined in its header.
WebIDL's schema generator emits dictionaries, escaping identifiers with a leading underscore; it
maps arrays to `sequence<any>` and rejects null-only fields. WIT supports only
bool/f64/string fields and lowercase underscore-separated names, translating
underscores to hyphens and escaping names with `%`. Each absent/nullable
dimension adds an `option` layer. Output fragments need their enclosing WIT
interface/package supplied by the application.

`amd64` owns the existing dsymspec AMD64 assembler context; supported encodings
remain exactly those of `assembler`. `elfbuilder` forwards to the existing
owned ELF64 relocatable builder. `coffbuilder` and historical `machobuidler`
write deterministic AMD64 objects with one executable text section. No symbols
or relocations are generated. `pebuilder` writes a fixed-base PE32+ console
image with one text section, no imports, relocation directory, runtime or unwind
tables; code must be freestanding and position-independent. The entry offset
must lie inside the supplied code. Builders emit files, never map or execute them.

## HTTP and policies

`http_request`, `http_response` and `http1` reuse the existing strict HTTP/1.1
model and framing validator. Constructors borrow input and publish owned messages
only on success. Free them with `mulib_http_message_free`. `web_conntection`
retains bounded incremental input and removes exactly one successfully parsed
message per `next`; pipelined bytes remain. EOF is permanent. Limits and parsing
options are caller-configured. `httpserver_config` supplies explicit defaults,
not listener creation or timeout enforcement.

`http2` reads/writes nine-byte frame envelopes with per-type length/stream/padding,
priority dependency, promised-stream and SETTINGS value checks. HPACK,
continuation sequencing, settings application and flow control remain connection
responsibilities. `http3` validates varint envelopes, fixed identifier payloads,
reserved types/settings and duplicate SETTINGS identifiers; it does not implement
QPACK or stream roles. HTTP/3's temporary SETTINGS ID allocation can return -3.
`quic` supplies RFC 9000 varints and length-bearing STREAM frames, not packets,
TLS, authentication, loss recovery or congestion control. Unknown HTTP frame
types are preserved except explicitly forbidden HTTP/2 types in HTTP/3.
Protocol readers return 1 success, 0 truncated and negative
errors, leaving outputs unchanged otherwise. Frame data borrows input.
No module here provides TLS or claims to be an Internet-ready server.

Policy building blocks are single-threaded and explicit:

- `upstream` borrows host names and tracks in-flight capacity. Selection in
  `load_balancer` does **not** acquire; balance acquire/release yourself.
- `health_checker` applies consecutive-success/failure hysteresis to externally
  performed probes. No active probe scheduling or networking is implied.
- `rate_limit` uses caller-supplied finite, monotonic seconds. Denial may still
  update refill time/tokens; invalid/backward input changes nothing.
- `acl` is first-match IPv4/IPv6 CIDR policy, with strict parsers and matched-rule
  diagnostics. IPv4 uses numeric network-significance addresses, not native
  `in_addr` memory order. Invalid rules (even unreachable ones) deny everything.
- `waf` is an exact method allowlist plus size and strict path checks on an
  already parsed HTTP request. It is **not** a SQL/XSS/body-signature scanner.
- Historical `seurity` supplies HTTP token validation, ASCII equality,
  length-fixed byte comparison without early exit, and strict absolute paths.
  No compiler-independent constant-time guarantee or cryptography is claimed.
  Path checking decodes percent escapes once and rejects encoded separators,
  nested escapes, controls, backslashes and dot segments; it is a restrictive
  policy, not a universal URL canonicalizer.
- `auth` supplies bounded Basic/Bearer parsing, explicit verifier callbacks and
  opaque API-key comparison. Require authenticated TLS externally. `oauth2`
  supplies S256 PKCE, exact scope matching, expiring one-use state and claim
  policy checks. Its explicit verifier must authenticate tokens; missing policy
  denies. Applications supply CSPRNG nonces/verifiers, trusted clocks and durable
  synchronized replay storage. `otpauth` supplies HOTP/TOTP with replay-aware
  verification; `pwdauth` supplies salted PBKDF2-SHA-256 records. `authcrypto` is
  unaudited dependency-free cryptography, not a certified security provider.
  No JWT engine, OAuth server, token refresh or network client exists here.
- `vhpst` is interpreted as the requested virtual-host helper: exact ASCII
  DNS-name matching only, without ports, IP literals, wildcards or IDNA.
- `rewrite_url` replaces a component prefix and preserves the raw query. It does
  not generate redirect headers. `mime` uses extensions only, never sniffing.
- Historical `rpiter. static_file` uses the existing lexical VFS, strict paths,
  MIME lookup and owned responses. No native directory traversal, indexes,
  ranges or symlink security claims. A failed lookup returns INVALID; callers
  choose how to map VFS/HTTP errors to public responses.
- `compression` implements explicitly selected identity and MuLIB RLE, **not**
  HTTP gzip/deflate/Brotli negotiation. Decoders require an output limit.
- `metrics` has saturating counters and latency totals, no global registry,
  synchronization or network exporter.

`evtloop` is a cooperative one-shot task queue driven by absolute uint64 ticks.
Set a nonzero task limit, pass monotonic time, and bound each run by a callback
budget. Equal due times execute in submission order. Callbacks can post/cancel,
but cannot free the loop; recursive run fails. It is not an OS descriptor poller.
`mulib-mulib.h` is intentionally a small core umbrella and version API, not an
include-everything switch that silently enables platform dependencies.

## Ownership, errors and validation

Zero-initialize owned objects, configure dimensions/limits/schema before use,
never shallow-copy live owners, and free with the matching function. Schema,
callback and endpoint contexts are borrowed. No module is internally synchronized.
Buffer encoders/builders append atomically on failure, including aliased input;
text generators append bytes **without** a NUL terminator. String lookup results
are static or borrowed as documented. JSON decode results are caller-owned.
Do not overwrite a live output object without freeing it first.

```sh
python3 tools/check_storage_server.py
CC=clang CXX=clang++ python3 tools/check_storage_server.py --sanitizers
```

Checks cover every module's isolated dependency closure, repeated includes,
separate C translation units, C++ declarations, known wire vectors, truncation,
hostile mutation, allocation failures, storage sequencing, policies and HTTP
composition. `llvm-readobj` and Python `msgpack` add optional independent output
checks. The wider repository currently also references 11 absent legacy OCI/image
headers; these focused checks intentionally do not erase those unrelated references.

## Direct dependencies

Implementation switches are `MULIB_IMPL_` plus the uppercase module name with
hyphens changed to underscores, except `seurity` also accepts `SECURITY`,
`web_conntection` uses `WEB_CONNECTION`, and `rpiter. static_file` uses
`STATIC_FILE`. These direct edges include declaration dependencies; enable the
full transitive closure as shown by the standalone examples. This table is
separate from the global strata generator, which currently stops at the absent
legacy `ocicontainerruntime` header.

| Module | Direct dependencies |
| --- | --- |
| amd64 | assembler |
| row-rdbms | documentdb, dbschema |
| column-rdbms | none |
| graphdb | memtable |
| objectdb | memtable |
| embeddingsdb | none |
| documentdb | json, memtable |
| dbserializer | bson, cbor, messagepack |
| bson | buffer, json |
| messagepack | buffer, json |
| protobuf | varint, buffer |
| dbschema | json |
| webidl | dbschema, buffer |
| muidl | dbschema, buffer |
| wit | dbschema, buffer |
| cbor | buffer, json |
| dbdesializer | dbserializer |
| mulib | buffer, strbuf |
| elfbuilder | objfilebuilder |
| machobuidler | buffer, bycutils |
| pebuilder | binfilewriter, bycutils |
| coffbuilder | buffer, bycutils |
| evtloop | none |
| vhpst | seurity |
| rpiter. static_file | vfs, mime, http_response |
| rewrite_url | seurity |
| compression | buffer, rle |
| upstream | none |
| load_balancer | upstream |
| health_checker | upstream |
| rate_limit | none |
| waf | http, seurity |
| acl | inet6 |
| auth | base64, seurity |
| seurity | none |
| oauth2 | auth, authcrypto |
| metrics | none |
| httpserver_config | http |
| http_request | http |
| http_response | webframework |
| mime | seurity |
| web_conntection | http1 |
| http1 | http |
| http2 | buffer |
| http3 | quic |
| quic | buffer |
