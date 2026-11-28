# Web-core staging audit

Scope: the Web-core entries in `MANIFEST.md.additions`, audited against
`a370cd51`. These headers are tracked C11 static-inline staging mechanisms
outside the base manifest inventory at that revision; they are distinct from
the registered [web primitives](web-primitives.md) and from browser renderer
event streams. No ECMAScript binding, provider trust, or standards conformance
is implied.

| Entries | Observed boundary and work still needed |
| --- | --- |
| `web-readablestream`, `web-writablestream`, `web-transformstream`, `web-streamqueue` | Readable is an owned linear byte FIFO with shifting reads; writable is a guarded synchronous sink; queue only accounts for byte size. No JS stream lifecycle, locks, BYOB, tee, cancellation or promise backpressure. Transform checks output size only after the callback, so the size limit cannot govern allocation by a provider. |
| `web-abort` | Caller-owned listener array and one synchronous abort notification; no removal, composition or scheduling. Active dispatch and context lifetimes need an explicit contract before an event-target adapter. |
| `web-structuredclone` | Byte copy and single staging ArrayBuffer backing-store move. No graph/identity table, cycle serialization, transfer-list validation or VM/realm integration. |
| `web-textcodec` | [Bounded streaming UTF-8 decoder](mulib-web-textcodec.md) now handles split scalars, exact malformed prefixes, fatal mode, BOM and cumulative output accounting. UTF-16-to-UTF-8 one-shot remains. No label table, JS/WebIDL binding, or stream graph. Separate `web-encoding` staging API is unchanged. |
| `web-blob`, `web-file`, `web-filereader`, `web-formdata` | Owned byte/name copying, signed slicing, synchronous Blob copy, and borrowed ordered FormData entries only. Missing MIME normalization, file backing, Blob/File-valued forms, multipart, async events and JS lifetime. |
| `web-messageport`, `web-broadcast`, `web-bloburl` | Host handle queue, storage-key/channel predicate, and caller-provided origin/URL registry lookup/revoke. No entanglement, delivery, serialization, origin authority, opaque name generation or lifetime integration. |
| `web-compressionstream` | Recognizes algorithm names; no compressor, decompressor, validation or resource limits. |
| `web-console`, `web-performance`, `web-reporting` | Host log callback, relative time arithmetic, and report records. No Console state/formatting, timing privacy, observers, or report delivery. |
| `web-eventsource`, `web-xhr`, `web-beacon` | Line-field split, partial state predicate, and quota arithmetic. No Fetch or transport, event dispatch, reconnection, CORS or delivery. |
| `web-crypto` | Existing callback dispatch and 65,536-byte request limit. No trusted entropy or SubtleCrypto provider is added. |

The text-codec slice imports only baseline staging helpers and `utf16`.
Registration/generation, whole-browser gates, and composition with P0
WebIDL, VM, event-loop, Fetch or native providers remain separate integration
work. Use only the documented offline profile.
