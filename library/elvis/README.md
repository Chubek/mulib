# ElviScrape providers

These are declarative inputs for [MuLIB ElviScrape](../../manual/mulib-elvi-scrape.md),
not executable Surfraw scripts or SearXNG engines. Use the filenames below
exactly as written; they are case-sensitive.

## Surfraw-derived HTML profiles

The request templates are based on the local `.sot/surfraw` checkout
(`90b4199a59fd5886c0e0b208e5c88472f5fc1768`), read as source only. Surfraw
builds browser URLs; the HTML extraction rules below are MuLIB's.

| Provider | Surfraw source | Adaptation |
| --- | --- | --- |
| [`Google.elvs`](Google.elvs) | `elvi/google:81–86,95–125` | Default US web search over HTTPS; `num={limit}`. Extracts H3 titles inside links; supports Google `/url` wrappers. No regional, specialty, safe-search, or lucky options. |
| [`Bing.elvs`](Bing.elvs) | `elvi/bing:23–27` | HTTPS with the script's `go=`, `form=QBLH`, and `filt=all`; retains the local provider's `count={limit}`. Extracts links inside H2 headings. Does not unwrap Bing tracking links. |
| [`DuckDuckGo.elvs`](DuckDuckGo.elvs) | `elvi/duckduckgo:8–17,53–82` | Uses the dedicated Lite host with `kg=g` and requests direct links (`kd=-1`). Extracts direct links in table cells without filtering CSS classes; skips DuckDuckGo wrappers. |
| [`Scholar.elvs`](Scholar.elvs) | `elvi/scholar:19–23` | HTTPS `/scholar?q=...`; extracts direct links inside H3 headings. Unlinked citations are not results. |
| [`MDN.elvs`](MDN.elvs) | `elvi/mdn:18–22`, `surfraw.IN:504–518,535–543` | Selects Surfraw's Google custom-search backend and prefixes the query with `site:developer.mozilla.org`. This is a search-engine hint, **not** a destination allowlist. `home` is the Google backend's homepage because `home` and `search` must use the exact same HTTPS host. |

## SearXNG-derived public profiles

Endpoint and response-field provenance is from the local `.sot/searxng`
checkout (`b867c56a5668966ef4be8097f1b559bdc2841743`), read as source only.
These profiles issue single public HTTPS GETs; they use neither API keys nor
login cookies, private endpoints, JavaScript, or challenge workarounds.
Source paths in this table are relative to that checkout.

| Provider | SearXNG source | Adaptation |
| --- | --- | --- |
| [`Crossref.elvs`](Crossref.elvs) | `searx/engines/crossref.py:20–44,54–86` | Searches the public works API for scholarly metadata. Requests `rows={limit}` with only `title,URL`; reads `/message/items`, first title (`/title/0`), and DOI link (`/URL`). No record-type filtering, fallback title, or publisher URL override. `home` is on the API host to keep the request host fixed. |
| [`FDroid.elvs`](FDroid.elvs) | `searx/engines/fdroid.py:9–32,35–51` | Searches the public F-Droid HTML page, with the first page and default language. Requires the case-sensitive `package-header` anchor class token and takes its H4 title and direct HTTPS package link. |
| [`Hex.elvs`](Hex.elvs) | `searx/engines/hex.py:7–25,51–69` | Searches the public Hex package API, first page sorted by recent downloads, with `per_page={limit}`. Reads the root result array (`results /`), `/name`, and the package page link `/html_url`, not the API resource URL. |
| [`LibRS.elvs`](LibRS.elvs) | `searx/engines/lib_rs.py:7–29,34–46` | Searches the public Rust package HTML page. Extracts H4 titles within links and resolves root-relative package links on `lib.rs`. The H4 rule is narrower than accepting every anchor but does not reproduce SearXNG's full XPath ancestry. |
| [`OpenLibrary.elvs`](OpenLibrary.elvs) | `searx/engines/openlibrary.py:38–65,69–91` | Searches the public catalog for books. Requests the first page with `limit={limit}` and only `key,title`. Reads `/docs` and resolves its root-relative `/key` values on `openlibrary.org` using the same-host rewrite rule. |
| [`Packagist.elvs`](Packagist.elvs) | `searx/settings.yml:1885–1903` | Searches the public PHP package API on the first page, with `per_page={limit}`. Reads `/results`, `/name`, and `/url` using the configured `json_engine` fields. |

## Behavior and verification

`{query}` is encoded as a URL component. Every profile accepts limits from 1 to
30 and caps local extraction at 30 entries; only templates containing `{limit}`
send that number to the service. An empty query selects `home`; that URL need
not return results or match the search response's media type. HTML profiles
parse static markup; they cannot handle consent or bot-challenge pages. Only
profiles opting into `rewrite relative` resolve root-relative links;
scheme-relative links such as `//example.org/path` are not accepted.
ElviScrape does not follow result links, redirects or pagination. Markup
changes and service-side limits can reduce the result count.

Provider tests use synthetic responses, not captured live pages. They check
query encoding, exact request URLs, and ordered output, including duplicate
and unusable results. The SearXNG-derived profiles also exercise the search
API with an offline fixture transport.

Live smoke probes (2026-11-19) used URLs rendered by the ElviScrape demo,
curl with verified TLS and no redirects, then the demo to parse successful
responses. Through `socks5h://0.0.0.0:30301`, Crossref (`muon`), F-Droid
(`firefox`), Hex (`phoenix`), Open Library (`pride and prejudice`), and
Packagist (`monolog`) each returned HTTP 200 and 20 parsed results. Lib.rs
(`serde`) returned HTTP 403 and was not retried through SOCKS or parsed. There
was no direct fallback. The HTTP proxy at `http://0.0.0.0:31302` also supported
verified HTTPS CONNECT; the plain-HTTP CONNECT test at `:31303` aborted, and a
TLS-to-proxy probe there did not pass certificate verification.

From the repository root:

```sh
make build/test-elvi-scrape-providers
./build/test-elvi-scrape-providers
```
