# ElviScript profile grammar

ElviScript uses the same `.elvs` compiler as
[ElviScrape search profiles](mulib-elvi-scrape.md). Compilation builds a
declarative program; it does not resolve variables, load dependencies, execute
Lua, make requests, create artifacts, or connect to IPC.

## Scrape mode

```text
elvi booking
host booking.com
home https://booking.com
mode scrape
cookies provide{$HOME/.mulib/elvi/cookies/BookingCookies.json}
xpath provide{mulib-xpath}
artifacts discard
logic lua{$HOME/.mulib/elvi/logic/BookingDotCom.lua}
forward ipc{/dev/shm/elv-scrape/BookingDotCom}
```

Every directive in this example is required except `cookies`, which can be
omitted for profiles that do not need credentials. Directives may appear in
any order, once each. Names and provider keywords are case-sensitive. `host`
is a DNS hostname; the compiler folds its ASCII letters to lowercase. `home`
must pass the same URL gate used by the runtime.

| Directive | Accepted value | Compiled stage |
| --- | --- | --- |
| `cookies` | `provide{PATH}` | `MULIB_ELVI_STAGE_COOKIE_FILE` |
| `xpath` | `provide{mulib-xpath}` | `MULIB_ELVI_STAGE_XPATH` |
| `artifacts` | `discard` | `MULIB_ELVI_STAGE_DISCARD` (no argument) |
| `logic` | `lua{PATH}` | `MULIB_ELVI_STAGE_LUA` |
| `forward` | `ipc{PATH}` | `MULIB_ELVI_STAGE_IPC` |

`PATH` is a nonempty string, not a command. The Lua and IPC expressions
name resources, not inline code or a request to open a socket. The caller
must separately authorize whatever it supplies to the runtime.

## Document mode

```text
elvi article
host www.example.org
home https://www.example.org/article
mode document
artifacts capture
```

The distinct document profile requires `artifacts capture`. It is not a
variant of `artifacts discard`. It may declare `cookies provide{PATH}` but
rejects `xpath`, `logic`, `forward` and all legacy search directives. As in
scrape mode, `elvi`, `host` and `home` are required, the exact-host HTTPS URL
gate applies, and compilation performs no I/O. The trusted host performs
separate host and optional cookie-path authorization before invoking the
[document transport](elvi-script-transport.md#capture-a-document).

## Lexical rules

- The file contains one directive per line; an open provider expression can
  span physical lines. Blank lines and `#` comments are ignored outside
  quotes. In a provider expression a comment ends at the next LF, not at a
  closing brace inside that comment. LF and CRLF line endings are accepted.
- The opening brace directly follows the provider keyword on the directive's
  opening line. The matching outer braces are not part of the argument.
  Nested braces are balanced and retained as text, not evaluated. A trailing
  comment is allowed after the outer closing brace, but not another token.
- Inside braces, single or double quotes preserve whitespace and make `#`,
  `{` and `}` literal. Quotes themselves are removed. Quoted text cannot
  span physical lines. Unquoted runs of whitespace (including line breaks)
  become one space; unquoted leading and trailing whitespace is discarded.
- Inside or outside quotes in braces, a backslash quotes one of `\`, `"`,
  `'`, `{`, `}`, `#`, or an ASCII space. Other escapes and control bytes in
  argument text are rejected. Legacy non-provider values retain the original
  token grammar, including double quotes with `\\` and `\"`.
- Sources are at most 64 KiB and cannot contain NUL bytes. Each decoded stage
  argument is at most 1023 bytes, plus its terminating NUL. Brace depth is
  at most 32, counting the outer pair.

For example, `logic lua{ "$HOME/providers/{local}/file #1.lua" }` stores
`$HOME/providers/{local}/file #1.lua`. Quotes change lexical interpretation
only: they do not suppress eventual validation or HOME expansion.

## Compiler and policy APIs

`mulib_elvi_scrape_compile` reports errors using the one-based physical
source line and a message; line zero denotes an input/file-level error
(or absent source). Invalid syntax or validation failure leaves the
destination program unchanged. Each populated stage also records its
opening directive line. `mulib_elvi_scrape_load` reads only the explicit
profile file before calling the same compiler.

`mulib_elvi_script_resolve` substitutes only `$HOME` or `${HOME}` at the
start of a path, followed by `/` or the end of the argument. The caller supplies
the approved absolute HOME; there is no environment lookup, tilde
expansion, variable interpolation elsewhere, or shell evaluation. Other
dollar forms are rejected. Literal paths do not require a supplied HOME.
Resolution leaves output unchanged if it fails and does not canonicalize
paths, inspect files, or confer authorization.

`mulib_elvi_script_url_allowed` accepts only absolute public HTTPS URLs with
the compiled exact host (case-insensitive), without userinfo, explicit
ports, fragments, or malformed/unsafe URL escapes. It performs no DNS
resolution; an actual transport must validate its destination and peer.

## Legacy search mode

Omitting `mode` is equivalent to `mode search`; existing HTML and JSON
profiles still work. Scrape and document stages are not allowed in search mode, and
legacy search-specific directives (`search`, `anchor`, `rewrite`,
`exclude_host`, `max_results`, `format`, `anchor_class`, `results`, `title`,
`url`) are not allowed in scrape or document mode. The legacy search, URL
rendering and result extraction entry points require search mode.
