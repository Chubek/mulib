# ElviScript scrape coordinator

ElviScript runs `mode scrape` profiles. Legacy `mode search` remains a
separate [ElviScrape](mulib-elvi-scrape.md) API. The profile names stages;
it is not permission to access files, sockets, or a network.

- `mulib_elvi_script_run` takes an unmodified compiled profile, an injected
  synchronous transport, authorized Lua source bytes and a connected Unix
  stream. It reads no cookie file. The application binds each opaque
  capability to its independently approved absolute path.
- `mulib_elvi_script_run_https` is an explicit opt-in using a caller-supplied
  verified TLS adapter. If the profile includes cookies, a separately
  approved absolute path must match the expanded profile path and pass the
  [transport loader](elvi-script-transport.md). No built-in live connector
  or current-provider support is supplied.
- Path expansion uses only caller-approved HOME, without environment access.
  The coordinator never opens the Lua or IPC path. A supervisor can provide
  a socketpair or authorize and connect a named endpoint on its own.

The coordinator calls the optional start hook, issues GETs in FIFO order
(the validated homepage is first), and calls the response hook for every
accepted response. It rejects duplicate URL strings, more than eight total
requests, cross-host URLs, changed effective URLs, non-200 responses, missing
or duplicate HTML media types, non-identity body encoding, and empty or
oversized bodies. No redirects or retries. The in-memory body and headers are
freed after their hook, including on failure (`artifacts discard`). The
optional finish hook runs after the queue drains and cannot enqueue or emit.
[Logic](elvi-script-logic.md) is supplied by the trusted application; these
controls do not make arbitrary third-party Lua safe to run in process.

`discard` is the only artifact directive. An embedding that opts into
retention owns it outside the engine: keep an explicit allowlist of sanitized
fields, use a private destination and size/age limits, and never retain cookies,
raw HTTP headers, or unreviewed response bodies. By default the engine persists
nothing; result persistence is a receiver policy.

Results obey the profile cap and are staged until logic succeeds. They remain
tentative for the receiver until a valid completion message. There is no
automatic replay after partial transmission. Success means the sender wrote
completion, not acknowledgement or persistence by the receiver.

Use the full one-translation-unit build recipe and synthetic fixtures in
[`tests/test-elvi-script.c`](../tests/test-elvi-script.c) as a reference.
The test never connects to the internet, including when exercising authorized
cookie loading and HTTPS composition (its TLS callbacks are in memory). It
prints the two synthetic accommodation records received from the fixture.

```sh
make check-elvi-script
```

### Agoda

[`Agoda.elvs`](../library/elvis/Agoda.elvs) starts at the public Singapore
listings URL on `www.agoda.com`. The host supplies
[`Agoda.lua`](../library/elvis/logic/Agoda.lua) as the approved
`$HOME/.mulib/elvi/logic/Agoda.lua` source and connects the approved
`/dev/shm/elv-scrape/Agoda` Unix stream. The profile uses no cookies.
The script selects static `data-selenium="hotel-item"` cards and emits name,
same-host URL, optional display price and optional location. It handles
root-relative links and deduplicates by URL without requesting more pages.
The synthetic Agoda fixture runs in the same test above. Live compatibility
is unverified; the profile does not render JavaScript or handle challenges.

See also [profile grammar](elvi-script-grammar.md) and the
[IPC wire and descriptor contract](mulib-elvi-script-ipc.md). The
[coordination archive](elvi-script-mailbox/README.md) records subsystem
interfaces, test evidence, and remaining limits.
