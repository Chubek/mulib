# SOAP 1.1 and 1.2

[Manuals](README.md) · [Library index](index.md)

Include [`mulib-soap.h`](../include/mulib-soap.h). Define `MULIB_IMPL_CDD_SOAP`
in one C translation unit and link [MuW3](mulib-muw3.md) and
[XML](mulib-xml.md) (`MULIB_IMPL_XML`, `MULIB_IMPL_STRBUF`,
`MULIB_IMPL_UTF8`). The `MULIB_IMPL_SOAP` spelling and original
`mulib_soap_*` entry points remain available.

## Construct and inspect

- `mulib_cdd_soap_escape_text` validates XML 1.0 UTF-8 and escapes
  character data. It rejects malformed UTF-8, embedded NUL and disallowed
  XML characters; it does not escape element/attribute names or URIs.
- `mulib_cdd_soap_envelope` wraps trusted XML fragments in an XML declaration,
  Envelope, optional Header and required Body. Pass zero for SOAP 1.1 or
  nonzero for SOAP 1.2. NULL or empty Header omits it; Body cannot be NULL.
  Both helpers return malloc-owned UTF-8 text or NULL. The length bound
  excludes the NUL terminator; zero selects 1 MiB.
- `mulib_cdd_soap_inspect` validates the byte span as XML and checks the
  selected version's Envelope, Header/Body order and Fault structure. SOAP
  names are matched by in-scope namespace URI and local name, not by prefix.
  A direct SOAP Fault must be the only Body element. For SOAP 1.1 the
  faultcode QName and faultstring are required. For SOAP 1.2 the standard
  Code QName, optional Subcode chain, and Reason with at least one
  `Text xml:lang="..."` are checked. A payload element named `Fault` in
  another namespace is not treated as a SOAP Fault.

## HTTP call

`mulib_cdd_soap_call` validates the request, POSTs it with MuW3 and checks
the response. SOAP 1.1 uses `text/xml` with a quoted `SOAPAction` header
(empty when NULL); SOAP 1.2 uses `application/soap+xml` with an optional
quoted `action` media parameter. Actions may contain at most 240 visible
ASCII bytes and cannot include quotes, backslashes or whitespace; a SOAP
1.2 action must be nonempty when supplied.

The response must use the version's media type, UTF-8 (explicit or implicit),
and absent or `identity` content encoding. Normal envelopes require HTTP 2xx;
faults require HTTP 500 (SOAP 1.1), or HTTP 400 or 500 (SOAP 1.2).
`1` denotes a valid response including faults; use optional `is_fault` and
the returned HTTP status to distinguish them. `-1` indicates invalid or
unsupported input/response, `-2` is a size limit, `-3` is allocation failure,
and other MuW3 errors are passed through. Outputs are unchanged on error.
Free the response with `mulib_muw3_response_free` before reuse. The XML byte
limit applies alongside MuW3's configured HTTP body limit.

The legacy `mulib_soap_call` still accepts a prepared raw XML string and
returns the raw MuW3 response without XML or status validation. Use it if the
service intentionally returns empty or non-SOAP responses.

This is a bounded UTF-8 structural subset, not a SOAP processor or schema
validator. XML parsing inherits the kernel's ASCII name subset, nesting cap,
and rejection of DTDs, external entities and processing instructions.
SOAP also limits each element to 128 attributes (including namespace
declarations). No
WSDL, XSD, WS-Security, `mustUnderstand` processing, application-level
namespace/schema validation, MTOM/MIME attachments or fault-code-specific
HTTP status mapping is implemented.

[Complete example](../examples/modules/soap.c) ·
[Focused tests](../tests/test-api-soap.c)

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  examples/modules/soap.c -pthread -lm -ldl -o soap-example
```

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-soap.h`](../include/mulib-soap.h) |
| Implementation switch | `MULIB_IMPL_CDD_SOAP` |
| Repository-local includes | [mulib-muw3](mulib-muw3.md), [mulib-xml](mulib-xml.md) |
| Complete example | [`examples/modules/soap.c`](../examples/modules/soap.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/soap.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
