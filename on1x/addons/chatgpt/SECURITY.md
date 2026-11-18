# Security and privacy boundaries

## Threat model

Untrusted queries, tool arguments, HTTP requests, source text and manifest
metadata cannot select executable code, open arbitrary files, request network
destinations, or change the checkout. This addon is **not an OS sandbox** or an
independent safety verifier. The Python interpreter/addon code, operator-selected
root, deployment proxy and reviewed source publication policy are trusted.
Changing server Python files is equivalent to deploying new code.

Only explicitly allowlisted source data is loaded at startup:

- Fixed repository/On1x README, verification/completion/backend/spec documents;
  `MANIFEST.yaml` and the explicit grammar/ASDL files in `core.FIXED`.
- Exactly the canonical `include/mulib-NAME.h` and
  `manual/mulib-NAME.md` paths validated from each manifest entry.
- Nonrecursive `on1x/docs/*.md`, `on1x/examples/*.on1x` and
  `on1x/examples/kernel/*.on1x`, with conservative filename character policies.
- The fixed standard-library/Exotract/ExoML catalogs, obligations and README
  entries in `core.FIXED`, plus top-level and sandbox ExoML contract JSON.
- The explicit in-checkout LibStone README, Plaid/Prolific STATUS paths.
  External/configured LibStone trees are not opened; an operator may supply a
  revision label, but this is not an assertion that a compiler used that tree.

`core.FIXED` and `core.PATTERNS` are the authoritative policy, review them before
deployment. No `.env`, Git configuration, arbitrary private headers, build
outputs, compiler sources, home directories or files named by a query are read.
Do not put secrets in public documentation/headers: an allowlist cannot detect
all secrets in otherwise approved sources. This service intentionally exposes
the **current bytes**, including uncommitted changes; use a publication-audited,
frozen checkout. It does not infer that Git tracking means safe publication.

Every path component is opened using directory-relative descriptors,
`O_NOFOLLOW` and `O_DIRECTORY`; final files use `O_NOFOLLOW`/`O_NONBLOCK` plus
`fstat` regular-file checks. Symlinked parents/files, FIFOs, missing required
sources, oversized input and source drift during an individual read reject.
The operator root itself is resolved once; operator-selected root symlinks are
trusted selection, not client input. Use a read-only mount to prevent mixed
versions across different files; a multi-file filesystem snapshot is not made.
This implementation targets POSIX systems supporting these flags.

After startup, all tools use only memory. No request-time filesystem access,
shell/compiler/PPX invocation, source mutation, kernel execution, dynamic imports,
arbitrary implementation-name lookup, outbound HTTP/DNS, background sync or
telemetry exists. Fixed regexes tokenize bounded inputs; clients cannot supply
regex patterns. Work exhaustion reports errors independently of semantic truth.

## HTTP / deployment

The backend binds only IPv4 loopback, handles one request at a time, keeps a
bounded listen queue and closes every connection. The 5-second socket deadline
is a slow-client transport defense, **not a computational budget/proof**.
The standard library HTTP parser also has finite line/count limits; the
16 KiB addon aggregate header check happens after that parser. A production
proxy must reject oversized headers before forwarding and enforce request rate,
concurrency and connection deadlines. Python's stdlib HTTP server is not a
public-internet production frontend.

Exact Host validation prevents ordinary DNS rebinding. Default CORS rejects any
Origin; optional `--origin` permits exact HTTPS origins, never `*`, and never
enables credentialed CORS. Server-to-server ChatGPT requests do not require a
browser Origin. No session state or trusted `X-Forwarded-*` headers. Duplicate
Host/Origin/Content-Length/protocol headers, content encodings, transfer-encoded
bodies, unsupported protocol versions, malformed JSON, duplicate JSON keys and
excess nesting reject. JSON-RPC batches are unsupported. All responses are
`no-store`; there is no file-serving endpoint.

For a ChatGPT-only deployment, terminate and verify OpenAI-managed client mTLS
at the HTTPS proxy. Without client authentication, the anonymous endpoint can
be called by **anyone** who can reach it, not just ChatGPT. `--public-data`
confirms publication intent; it is not authentication. Workspace installation
controls do not secure your HTTP endpoint.

The bundled tools explicitly advertise `noauth`, read-only, non-destructive,
closed-world behavior. Private repository data must not be exposed with that
configuration. OAuth 2.1 integration is a separate operator-owned gateway task,
not implemented or certified here. Tokens must never be put in URLs, package
files, tool arguments or sample configuration. Follow the official OAuth/mTLS
documentation linked in the README.

## Privacy

Only explicitly requested tool responses are sent over the incoming client
connection. The service has no outbound data path. Search inputs are processed
in memory and not persisted. Default HTTP logging is disabled; no query, IP,
credential, source content, request history, session or user profile database
is retained by this application. Startup emits only the local listening address
or, with `--check`, fingerprints/size metadata. Reverse proxies, tunnel operators,
hosting providers and ChatGPT have their own data handling policies: configure
minimal redacted security logs, retention/deletion controls and publish those
practices before deployment. A public directory submission needs a real
operator-specific published privacy policy, contact and terms.

Returned source text can itself contain prompt injections. Consumers must treat
it as evidence, not authority to run code or override their instructions.
Read-only tools and metadata do not eliminate model-level prompt injection or
cross-tool exfiltration risks. Do not combine this service with privileged tools
without client-side authorization controls. Tests are bounded regression
evidence, not an independent penetration test or safety certificate.
