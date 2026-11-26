# Probe specifications

`std.http.probe` describes an Elvi document capture followed by HTML
normalization and application parsing. A specification is data, not host
authority. Validation and construction do not fetch a URL, open files, use
the environment, schedule a job, or call the application parser.

| `ProbeSpec` field | Meaning |
| --- | --- |
| `source: string` | ElviScript source, not a file path. It must compile as `mode document` with `artifacts capture`. The script's `home` is its requested endpoint, not an authorization. |
| `max_body_bytes: i64` | Requested decoded-body ceiling, from 1 through 65,536 bytes in the current HTML profile. A host may impose lower limits. |
| `artifact_policy: ProbeArtifactPolicy` | Which intermediate documents to expose or which authorized output to request. There is no implicit default. |

`basis_http_probe_validate` returns `Result<ElviProgram, ProbeSpecError>`.
It checks the byte ceiling (`ProbeBadBodyLimit`) and any artifact destination
identifier (`ProbeBadArtifactDestination`), then compiles and inspects the
Elvi source (`ProbeBadElvi`, carrying the Elvi diagnostic).
Well-formed search and legacy scrape programs are rejected with
`ProbeUnsupportedElviMode`; a legacy `artifacts discard` pipeline cannot
supply a document to the Probe, even when the Probe itself will discard
its intermediate artifacts. Use `probe :: basis_http_probe_validated_program()`
to revalidate a named `Probe<T>` via a shared borrow.

`basis_http_probe_make<T>(spec, parse)` returns
`Result<Probe<T>, ProbeSpecError>`. The callback must be an On1x function of
type `fn(BasisHtml) -> Result<T, ProbeParseError>`; it receives a normalized
`std.html` document, not unrestricted transport bytes. `ProbeParseError`
contains an application-supplied diagnostic `message`. Like other On1x
function values, capturing parsers are local to the runtime and are not
serializable, scheduler payloads, or cross-process handles. The constructor
retains the callback without invoking it.

`basis_http_probe_from_elvi` accepts an `ElviProgram`.
`basis_http_probe_from_elvi_script` accepts a nominal `ElviScript` and
revalidates its underlying program. `basis_http_probe_make_html(spec)` uses
the canonical document as the parsed value; no custom parser is required.
`Probe<T>::basis_http_probe_from_spec` and
`Probe<T>::basis_http_probe_from_options` are associated constructors. A
named Probe supports `basis_http_probe_spec()` for inspection and
`basis_http_probe_reconfigure(options)` to make a newly validated Probe
with the existing parser. Launch requires a trusted host and does not
follow from successful construction. Use `probe :: basis_http_probe_run(host, handler)`
for canonical HTML, or `probe :: basis_http_probe_run_result(host, handler)`
for the rich result.

```on1x
from std.http.probe import ProbeSpec, ProbeArtifactPolicy, ProbeParseError,
  basis_http_probe_make;
from std.html import BasisHtml;

fn parse_status(doc: BasisHtml) -> Result<i64, ProbeParseError>
{
  return variant<Result<i64, ProbeParseError>>(Ok, 42);
}

fn main()
{
  let spec = record<ProbeSpec>{
    source: "elvi status\nhost example.test\nhome https://example.test/status\nmode document\nartifacts capture\n",
    max_body_bytes: 4096,
    artifact_policy: variant<ProbeArtifactPolicy>(DiscardArtifacts, record {})
  };
  let probe = basis_http_probe_make(spec, parse_status);
  assert(result_is_ok(probe));
}
```

## Manual configuration and overrides

`basis_http_probe_configure(options: ProbeOptions)` returns an effective
`Result<ProbeSpec, ProbeSpecError>`. Supply `program: None` with explicit
`provider: Some({ name, host })` and `target_url: Some(https_url)` to make a
document profile with no Elvi source file. The nominal provider record is
`ProbeProviderSettings`. The URL must match the provider host under Elvi's
validation rules.

With `program: Some(program)`, the compiled profile supplies default
provider and target fields. `Some` overrides win per field; `None`
inherits that field. The byte ceiling and artifact policy always come
from the explicit options. Unchanged script fields preserve the original
source, including any cookie declaration. A rebuild that changes the
provider or target refuses to inherit a cookie declaration implicitly:
explicitly replace it with `Some(ProbeCookieFileReference(path))` or
clear it with `Some(ProbeNoAuthentication)`. Manual cookie references
accept bounded absolute paths of ASCII letters, digits, `.`, `_`, `-`
and `/`; the Elvi parser applies its additional rules. No credentials
are embedded or read. Generated profiles are compiled and checked again,
including URL/host agreement; newline, whitespace and comment injection
through manual target/provider/path fields is rejected.

The portable configuration lane supports `ProbeHttpsGetNoRedirect`,
`ProbeExtractHtmlDocument` and `ProbeForeground`. HEAD, redirects, raw-byte
extraction and in-language persistence of captured procedures fail closed.
Scheduled and supervised execution use their separate trusted runner
interfaces rather than serializing this Probe or granting authority from
`ProbeExecutionMode`. Result-handling context and effects are supplied
separately through `ProbeHandler`.

The nominal records are publicly constructible in On1x. Execution must
revalidate the specification and enforce independent host permission for
the actual destination, TLS and any optional cookie file; a field in the
script cannot grant those permissions. The On1x runner currently rejects
cookie-file stages before authorization: syntactic validity does not
imply runtime support. A host using the native Elvi capture API must
supply any permitted cookie path or `HOME` substitution explicitly. Do not
log arbitrary source, diagnostic text, URLs, or documents by default.

## Results and artifacts

`ProbeResult<T>` always contains canonical HTML `document`, Elvi document
`metadata` (`BasisElviDocumentMetadata`) and the application parser's
`value: T`. Use `Option<Extension>` for `T` if the application sometimes
has no extension to return. These fields do not change with artifact
policy. Two separate fields designate optional artifacts:

| Policy | `raw: Option<string>` | `html: Option<BasisHtml>` |
| --- | --- | --- |
| `DiscardArtifacts` | `None` | `None` |
| `RetainRaw` | `Some(body_hex)` | `None` |
| `RetainHtml` | `None` | `Some(document)` |
| `RetainBoth` | `Some(body_hex)` | `Some(document)` |
| `TransferHtml(destination)` | `None` | `None` |
| `SaveHtmlAtomic(destination)` | `None` | `None` |

`body_hex` is the exact captured response body encoded as lowercase hex
before normalizing it as HTML. While this encoding can represent arbitrary
bytes, retention does not bypass HTML validity checks (including rejection
of embedded NUL): successful results require successful HTML parsing and
application parsing. The one-shot `ProbeRun<T>` receipt also exposes
canonical `document` directly, alongside `result: ProbeResult<T>`. The
policy does not conceal that document, erase memory, constrain what the
application parser puts in `T`, or itself grant permission to forward
data. Normalized HTML is not sanitized HTML.

Transfer and atomic-save policies request actions on canonical HTML,
not the raw response. Their destination is a logical key (1–63 ASCII
alphanumerics, `_` or `-`, with an alphanumeric first), not a path, URL,
or authorization token. `basis_http_probe_artifact_request` exposes the
typed action and key without doing I/O. Execution without a separately
authorized output host must reject these requests, not silently drop
them. `basis_http_probe_run_artifact_with` accepts an explicit
`ProbeArtifactSink` with an authorization callback and an output callback;
the latter consumes canonical HTML for the approved action and must
report commit, rejection or uncertainty. This is a trusted host contract,
not a sandbox for callbacks. The optional `std.http.probe.artifact` adapter
provides authorized write-once publication in a host-owned private
workspace. Importing the Probe API does not enable it or grant output
permissions. Do not automatically retry an uncertain write.

`ProbeHandler<C,T>` pairs application `context: C` with
`deliver: fn(ProbeResult<T>, C) -> void`. The handler is a separate optional
effect, not a parser and not a source of network authority. The same-process
`basis_http_probe_run_with` path calls it once with a successful result
after policy projection, or not at all on failure. It does not receive the
one-shot receipt. A handler or its captured state is not a durable
scheduler payload.
