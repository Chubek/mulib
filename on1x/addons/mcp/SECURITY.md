# Security and privacy boundaries

## Threat model

For the knowledge-only profile, untrusted queries, tool arguments, HTTP requests, source text and manifest
metadata cannot select executable code, open arbitrary files, request network
destinations, or change the checkout. This addon is **not an OS sandbox** or an
independent safety verifier. The Python interpreter/addon code, operator-selected
root, deployment proxy and reviewed source publication policy are trusted.
Changing server Python files is equivalent to deploying new code.

Only explicitly allowlisted source data is loaded at startup:

- Fixed repository/On1x README, verification/completion/backend/spec documents;
  `MANIFEST.yaml` and the explicit grammar/ASDL files in `core.FIXED`.
- Exactly the canonical `include/mulib-NAME.h` and
  `manual/mulib-NAME.md` paths validated from each manifest entry, plus
  nonrecursive `include/mulib-*.h` matching the canonical name grammar.
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

After startup, all knowledge tools use only memory. No request-time filesystem access,
shell/compiler/PPX invocation, source mutation, kernel execution, dynamic imports,
arbitrary implementation-name lookup, outbound HTTP/DNS, background sync or
telemetry exists. Fixed regexes tokenize bounded inputs; clients cannot supply
regex patterns. Work exhaustion reports errors independently of semantic truth.

## Private stdio VM execution

This opt-in profile must not be bridged to an anonymous or multi-user
endpoint. The process's stdin/stdout connection is the authority; keep
the client and operators trusted. The enabled tools cannot select paths,
compiler flags, host commands, or native libraries. The pinned compiler
builds standalone source without host-side activation inside a
fail-closed bubblewrap namespace with an isolated network namespace, fixed
read-only toolchain inputs, and no KVM device. Compiler wall time,
per-process CPU/address space and output-file size are bounded.

Guest execution uses the separate operator-selected ArchVirt helper and the
fixed DomIOS/KVM guest profile. The runner receives only the compiled ELF
and bounded serial records; there are no guest host filesystem mounts,
networks, argv, or environment. Neither the VM nor compile envelope is a
proof against defects in ArchVirt, KVM, the compiler, or host dependencies.
Run under a dedicated unprivileged account, and consider external cgroups
for host-wide process, memory and concurrent-session limits. Tool output
is untrusted, even when its terminal controls are escaped.

IDs authorize nothing outside the owning stdio process, and inputs/results
are not logged. A mode-0700 preprovisioned directory stores ELF artifacts
until explicit deletion or normal connection cleanup. Forced termination
can leave ELF files for operator cleanup but kills the runner via a Linux
parent-death signal.

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

`deploy.sh --cloudflared` explicitly creates an anonymous public Quick Tunnel
for testing with audited public data. It provides HTTPS, not client
authentication, a private URL, or the production proxy controls above. The
backend remains loopback-only and accepts the exact Host set by cloudflared;
that header is routing configuration, not an authorization token.

The bundled HTTP tools explicitly advertise `noauth`, read-only, non-destructive,
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
is retained by the knowledge profile. Private VM sessions retain only their
bounded live state, output and ELF files as described above. HTTP startup emits only the local listening address
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
