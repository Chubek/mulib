# On1x / MuLIB knowledge for ChatGPT

A read-only **remote MCP server**, not a compiler, safe-binding generator,
browser extension, or an OpenAI-endorsed product. Python 3.11+ standard library;
no API key, pip package, vector store, telemetry, or outbound network access.
The startup index is built from current canonical repository sources, never a
checked-in hand-maintained kernel inventory.

## Official integration choice (researched 2026-11-19)

Official OpenAI sources:

- [MCP server / Apps SDK quickstart](https://developers.openai.com/apps-sdk/quickstart):
  tools may run without a UI; Streamable HTTP supports stateless JSON responses.
- [Build an MCP server](https://developers.openai.com/plugins/build/mcp-server):
  tool schemas, read-only annotations, instructions, deployment and company
  knowledge compatibility.
- [Connect and test](https://developers.openai.com/plugins/deploy/connect-chatgpt):
  ChatGPT web custom-server registration, HTTPS or Secure MCP Tunnel, workspace
  permissions, installation and metadata refresh.
- [Authentication](https://developers.openai.com/plugins/build/auth):
  anonymous read-only data is supported; private data needs OAuth 2.1 enforcement;
  OpenAI-managed mTLS can authenticate ChatGPT as the client.
- [MCP search/fetch compatibility](https://developers.openai.com/api/docs/mcp):
  standard query/ID shapes, structured results and citation URLs.
- [Portable package format](https://developers.openai.com/plugins/build/plugins):
  root `plugin.json` and `mcp.json`, Agent Plugins schemas, remote
  `streamable-http` transport.
- [GPT Actions](https://platform.openai.com/docs/actions): another supported
  form for custom GPTs, deliberately not duplicated here.
- [Privacy/security](https://developers.openai.com/plugins/guides/security-privacy).

The current documentation calls this a **custom MCP server/plugin**, following
the Apps SDK path. This is **not** the retired 2023 `ai-plugin.json` plugin API:
no legacy descriptor is provided or served. Tool metadata is discovered through
`initialize` and `tools/list`; no OpenAPI file is required for MCP.

Compatibility: MCP **2025-06-18 and 2025-11-25**, Streamable HTTP with
`application/json` responses, stateless, no SSE listener. Newer clients can
negotiate the supported version. MCP 2.0 `server/discover`, events, tasks, UI,
resources and prompts are **not implemented**. The official
[MCP Events page](https://developers.openai.com/plugins/build/mcp-events)
distinguishes the newer MCP 2.0 flow from the still-supported initialization
flow; this service advertises only the latter. Real ChatGPT account/plan and
workspace availability must be checked at deployment.

## Local check and startup

From the MuLIB root:

```sh
python3 -B on1x/addons/mcp/test_chatgpt.py
python3 -B on1x/addons/mcp/server.py --root "$PWD" --check \
  --revision "$(git rev-parse HEAD)" \
  --libstone-revision "$(git -C on1x/third_party/libstone rev-parse HEAD)"
python3 -B on1x/addons/mcp/server.py --root "$PWD"
```

The **operator's shell**, not the service, invokes Git in the example.
The service never reads Git internals or invokes any subprocess. Omit revision
arguments to report `unknown`, never a fabricated revision. `--check` prints a
deterministic fingerprint/size report and exits. `--describe` prints the actual
tool descriptors (also requires `--root` syntactically; it does not read it).

By default the server binds **only `127.0.0.1:8787`**, accepting exact
`Host: 127.0.0.1:8787` or `localhost:8787`. Setting `--allowed-host` replaces
those defaults, not the loopback bind. No CORS origins are allowed by default.
Its index is an in-memory startup snapshot; restart to observe source changes.
Use an operator-controlled frozen/read-only checkout, not a concurrently edited
tree, for deployment. Startup rejects missing required sources and malformed
metadata instead of publishing a partially built index.

Local protocol check:

```sh
curl http://127.0.0.1:8787/mcp \
  -H 'Content-Type: application/json' \
  -H 'Accept: application/json, text/event-stream' \
  -d '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-11-25","capabilities":{},"clientInfo":{"name":"local-check","version":"1"}}}'
```

Subsequent POSTs use `MCP-Protocol-Version: 2025-11-25`. A
`notifications/initialized` notification gets HTTP 202 with no body;
`tools/list` lists all tools. There are no sessions or session cookies.

## Deploy and connect in ChatGPT web

**A local folder/ZIP is not a direct ChatGPT web installation and does not deploy
this Python server.** Run the service yourself; ChatGPT needs a reachable
endpoint or an explicitly configured Secure MCP Tunnel.

1. Audit every allowlisted source for publication. Choose a frozen checkout.
   Run the tests and `--check`; preserve its hashes and actual revisions.
2. To install the loopback service on a Linux systemd host with Python 3.11+,
   run from the MuLIB checkout (with the server's SSH host key already trusted):

   ```sh
   bash on1x/addons/mcp/deploy.sh root@server.example.org knowledge.example.org
   ```

   The installer saves the **committed `HEAD`** archive under `${TMPDIR:-/tmp}`
   and uses local Python 3 to check it before connecting over SSH. `deploy.sh`,
   `deploy_probe.py`, `server.py`, and `core.py` must be present in that archive
   and match the working-tree files. Staging changes in Git is not enough, and
   local edits or untracked files are never shipped. Use an audited checkout
   whose committed archive contains the deployment files before retrying.
   Passing `knowledge.example.org` approves the allowlisted data for anonymous
   publication and configures the exact Host accepted by the backend. Deploy
   checks `initialize`, `notifications/initialized`, and `tools/list` over
    loopback with that Host before keeping the new service. This mode does not
    install DNS, a TLS certificate, or an HTTPS proxy. Without either a public
    hostname or `--cloudflared`, it installs a local-only backend.
3. Put a maintained HTTPS reverse proxy in front of the loopback service.
   Enforce OpenAI-managed mTLS at that proxy to limit clients to ChatGPT
   (validate the documented chain, client-auth use and SAN; do not pin a leaf).
   Do not expose this stdlib HTTP backend to the public internet.
4. To run the anonymous backend manually instead of using `deploy.sh`, use:

   ```sh
   python3 -B on1x/addons/mcp/server.py --root /srv/mulib \
     --public-data --allowed-host knowledge.example.org
   ```

   Forward `Host: knowledge.example.org` unchanged, route only `/mcp`, use
   `https://knowledge.example.org/mcp` externally. Do not trust arbitrary
   forwarded-host headers. Set proxy request/header size limits, connection/
   concurrency/rate limits, TLS, and slow-client deadlines. Block outbound
   connections from the backend process. No proxy response-body/query logging.
5. **Private or unpublished sources are unsupported by the bundled anonymous
   configuration.** Before using them, supply an independently reviewed
   OAuth 2.1 resource-server gateway with protected-resource metadata, PKCE,
   issuer/audience/expiry/scope enforcement on **every** request and proper MCP
   challenges/tool security metadata. Keep this backend unreachable except
   through that gateway. Changing `mcp.json` alone does not implement OAuth.
   There is no builtin OAuth server, API-key hack, or token verification here.
6. Test your HTTPS endpoint with MCP Inspector (optional external development
   tool, not a runtime dependency). Select Streamable HTTP, inspect every
   schema, positive/negative case and both `content`/`structuredContent`.
7. In current ChatGPT web: [Plugins](https://chatgpt.com/plugins) → plus →
   **Add custom MCP server** → enter the public HTTPS `/mcp` URL, name, and
   **No authentication** for this anonymous configuration.
   Review the risk warning; **Create as a plugin**, install, start a new chat,
   type `@`, and select it. Workspace permissions apply. Older UI deployments
   may expose this under Apps/Connectors settings and developer mode.
8. Refresh the connection after schema/description changes. Re-run evaluation
   prompts and inspect the actual tool arguments/results. Public directory
   submission additionally requires a stable public deployment, domain/publisher
   verification, support details and a published operator-specific privacy
   policy; the bundled sample metadata is not a completed submission.

Secure MCP Tunnel is another official connection method for eligible clients;
its agent/authentication are not bundled or launched here. Local direct
`localhost` URLs are for protocol testing, not an assertion that ChatGPT cloud
can reach your machine.

### Temporary Cloudflare Quick Tunnel

For testing with **publication-audited public data only**, install a current
`cloudflared` on the remote systemd host, then run:

```sh
bash on1x/addons/mcp/deploy.sh --cloudflared root@server.example.org
```

Do not pass `PUBLIC_MCP_HOST` in this mode. The flag explicitly approves
anonymous publication: **anyone with the URL can call the tools**. The installer
starts a separate, unprivileged `on1x-knowledge-tunnel.service` and keeps the
backend bound to `127.0.0.1:8787` with an exact tunnel-origin Host. On success it
prints `https://<assigned-name>.trycloudflare.com/mcp` after testing MCP discovery
over certificate-verified HTTPS. Use the full URL in ChatGPT with **No
authentication**. This test does not verify ChatGPT workspace registration.

Stop the public endpoint with `sudo systemctl stop on1x-knowledge-tunnel.service`
on the server. The tunnel does not restart automatically or start at boot; a
redeploy stops the previous tunnel and produces a new URL if requested. If
startup or HTTPS discovery fails, the installer stops the new tunnel and rolls
back the backend deployment; it cannot restore a stopped temporary URL. For
diagnostics, run `journalctl -u on1x-knowledge-tunnel.service` on the server.

[Quick Tunnels](https://developers.cloudflare.com/tunnel/get-started/quick-tunnels/)
are not a production proxy, do not support SSE, and are distinct from the
Secure MCP Tunnel. The addon uses JSON response mode; for stable or
authenticated access, use a separately configured production proxy or tunnel.

Optional portable distribution (does **not** deploy the service):

```sh
sh on1x/addons/mcp/package.sh \
  --endpoint https://knowledge.example.org/mcp \
  --output /tmp/opencode/on1x-knowledge.zip
```

This creates a reproducible ZIP from an explicit addon-only file list, rewrites
the remote URL in `mcp.json`, includes no source inventory, secrets, caches,
build outputs or hooks, and refuses to overwrite an existing archive.
`mcp.json` in the source tree intentionally uses a nonworking `.invalid` host.
`plugin.json`/`mcp.json` follow the **current portable Agent Plugins** format,
not legacy `ai-plugin.json`; custom-server web registration does not need a ZIP.
Copying/extracting this addon anywhere works with an explicit `--root`.
The checkout, including allowlisted docs/catalogs, must remain available.

## Tools and evidence semantics

| Tool | Inputs | Result |
|---|---|---|
| `search` | `query` | Standard search results with excerpt IDs, titles, URLs |
| `fetch` | `id` | Complete bounded indexed excerpt, source path/lines/hash |
| `search_on1x` | `query`, optional `limit` | Language docs, examples and metadata |
| `explain_feature` | `query`, optional `limit` | Evidence for an explanation, not generated prose |
| `search_kernels` | `query`, optional `limit` | Canonical manifest names/purposes |
| `inspect_kernel` | exact `name` | Manifest entry, switch, dependencies, header/manual/contract excerpts, independent status fields |
| `inspect_capability` | `query`, optional `limit` | Scoped status/verification/blocker records |

Search is deterministic **keyword token matching**, not semantic search.
Use short identifiers/keywords; results rank matching tokens, then source path
and line. Kernel search AND-matches literal name/purpose substrings.
`fetch` IDs identify complete **excerpts**, not entire files; this intentionally
keeps the standard search/fetch shapes bounded. Search can find signatures
(`mulib_clamp_i64`), contracts, diagnostics, language features and examples.
Header text is **not** a C parser, ABI probe, contract proof, or safe binding.
No unvalidated serialized CDecl/pointer data is loaded.

Each excerpt has a relative canonical source path, one-based inclusive lines,
SHA256 of the **complete source file**, and a source URL. Every result also
carries the complete allowlisted snapshot fingerprint, operator-supplied
repository/LibStone revisions (or `unknown`) and a separate hash of available
allowlisted LibStone README/STATUS files, **not a full LibStone tree hash**.
All hashes use SHA256 and deterministic path ordering.

GitHub URLs refer to the operator's revision, or `master` when unknown.
They are **revision references, not a claim that dirty worktree bytes exist at
that URL**. Indexed hashes/text are authoritative for the snapshot. Publish
only a clean committed deployment checkout for public citation URLs that
exactly match source content. The service never contacts GitHub.

`inspect_kernel` separates:

- **Discovery:** manifest presence, not a callable API.
- **Implementation:** source manifest claim, not test execution.
- **ABI:** `NOT_CHECKED`; target checks never run.
- **Contract:** present/absent, exact header-pin match, never a certificate.
- **On1x admission:** `NOT_ESTABLISHED`; production review is never inferred.

Follow cited KernelBind/capability documentation for narrow raw/review-gated
profiles; no production review is shipped by this service. Historical status
rows are preserved verbatim and may describe superseded profiles. Do not
combine an old `ABSENT` row with a newer executable profile or promote
`EXECUTABLE`/`PARTIAL` to `VERIFIED`. Budget exhaustion is a distinct error, never
a proof, impossibility result or timeout-as-proof.

### Example evaluation prompts

- “Using On1x knowledge, explain `taskgroup within` and whether this is async/
  await. Cite current docs and blockers.”
- “Find the canonical clamp kernel. Show its implementation switch,
  dependencies and declared signature; distinguish that from On1x admission.”
- “Find the diagnostic `ON1X-KERNEL-NOT-ADMITTED` and explain the review gate.”
- “What does the current capability matrix say about recursive unique owners?
  Separate executable scope from unsupported MIR/FFI cases.”
- “Discover CDD registry documentation. Does extraction certify a safe import?”
- Negative: “Run this On1x/compiler/PPX/kernel”, “read `/etc/passwd`”, “fetch this
  URL”, “install a module”, “approve this contract”, or “search with regex”:
  none is supported.

## Limits / troubleshooting

See [security boundaries](SECURITY.md) and [measured verification](VERIFICATION.md).
Queries: 256 characters, 16 distinct tokens; results: 1–10 (default 5);
files: 4,096, total bytes: 32 MiB, file bytes: 1 MiB; chunks: 40,000,
24 lines/~3,500 UTF-8 bytes normally (one source line may reach 8,192
characters); postings visited/query: 100,000. HTTP body: 8 KiB, accepted
headers: 16 KiB, JSON depth: 48, full serialized response: 64 KiB including
duplicated MCP text/structured content. No silent partial index is returned.

No results: use exact identifiers or fewer keywords. `RESULT_LIMIT`/
`WORK_LIMIT`: narrow query/result count; the service does not claim exhaustive
knowledge. Startup `INVALID_MANIFEST`/`INVALID_JSON`/`INDEX_LIMIT`: repair canonical
metadata or review budgets offline, never relax input handling through prompts.
`UNSAFE_SOURCE`: replace symlinks with canonical regular files in the deployment
checkout. If ChatGPT refuses the connection, first check that the installer
succeeded and that the public HTTPS URL answers MCP POSTs through the proxy (or
tunnel). A loopback probe does not test public DNS, TLS/mTLS, proxy routing, or
workspace permissions. 403: review exact Host and Origin; do not enable wildcard
CORS. With the hostname above, add `-H 'Host: knowledge.example.org'` to the local
curl probe.
406: supply both MCP Accept types. GET 405 is intentional: no SSE listener.
Unknown `/well-known` routes return 404 because this anonymous backend is not
an OAuth provider. No external ChatGPT credentials or end-to-end web approval
were used in the offline tests.
