# ElviScrape providers

These are declarative inputs for [MuLIB ElviScrape](../../manual/mulib-elvi-scrape.md),
not executable Surfraw scripts. The request templates are based on the local
`.sot/surfraw` checkout (`90b4199a59fd5886c0e0b208e5c88472f5fc1768`), read as
source only. Surfraw builds browser URLs; the HTML extraction rules below are
MuLIB's and are tested with synthetic, representative markup.

| Provider | Surfraw source | Adaptation |
| --- | --- | --- |
| [`Google.elvs`](Google.elvs) | `elvi/google:81–86,95–125` | Default US web search over HTTPS; `num={limit}`. Extracts H3 titles inside links; supports Google `/url` wrappers. No regional, specialty, safe-search, or lucky options. |
| [`Bing.elvs`](Bing.elvs) | `elvi/bing:23–27` | HTTPS with the script's `go=`, `form=QBLH`, and `filt=all`; retains the local provider's `count={limit}`. Extracts links inside H2 headings. Does not unwrap Bing tracking links. |
| [`DuckDuckGo.elvs`](DuckDuckGo.elvs) | `elvi/duckduckgo:8–17,53–82` | Uses the dedicated Lite host with `kg=g` and requests direct links (`kd=-1`). Extracts direct links in table cells without filtering CSS classes; skips DuckDuckGo wrappers. |
| [`Scholar.elvs`](Scholar.elvs) | `elvi/scholar:19–23` | HTTPS `/scholar?q=...`; extracts direct links inside H3 headings. Unlinked citations are not results. |
| [`MDN.elvs`](MDN.elvs) | `elvi/mdn:18–22`, `surfraw.IN:504–518,535–543` | Selects Surfraw's Google custom-search backend and prefixes the query with `site:developer.mozilla.org`. This is a search-engine hint, **not** a destination allowlist. `home` is the Google backend's homepage because `home` and `search` must use the exact same HTTPS host. |

The existing lowercase paths `library/elvi-scrape/google.elvs` and
`library/elvi-scrape/bing.elvs` are byte-for-byte compatibility copies. Update
each pair together. Provider filenames are case-sensitive; Bing uses `.elvs`,
not `.evls`.

`{query}` is encoded as a URL component. Every profile accepts limits from 1 to
30 and caps local extraction at 30 entries; only templates containing `{limit}`
send that number to the service. An empty query selects `home`. Parsing works
on static HTML:
it does not run JavaScript, follow result links or pagination, or handle consent
and bot challenge pages. Markup changes, redirects, relative links (other
than recognized Google wrappers), and service-side limits can reduce the result
count to zero. Live service behavior is not part of these offline tests.

From the repository root:

```sh
make build/test-elvi-scrape-providers
./build/test-elvi-scrape-providers
```
