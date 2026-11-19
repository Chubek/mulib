/** @file mulib-soap.h
 * @brief Bounded UTF-8 SOAP 1.1/1.2 envelopes and HTTP calls.
 * Define MULIB_IMPL_CDD_SOAP in one TU (legacy: MULIB_IMPL_SOAP).
 */
#ifndef MULIB_SOAP_H
#define MULIB_SOAP_H
#include "mulib-muw3.h"
#ifdef __cplusplus
extern "C" {
#endif

/** Escape XML 1.0 UTF-8 text as a malloc-owned string. max_length==0 selects
 * 1 MiB. Lengths exclude the terminator. NULL on invalid input or failure. */
char *mulib_cdd_soap_escape_text(const char *text, size_t length, size_t max_length);
/** Wrap trusted XML fragments (not untrusted text). Validates their XML and
 * the resulting SOAP structure. NULL header or "" omits Header; body can be
 * empty but not NULL. Returns malloc-owned text; max_length==0 means 1 MiB. */
char *mulib_cdd_soap_envelope(int soap12, const char *header_xml,
    const char *body_xml, size_t max_length);
/** Inspect a UTF-8 SOAP envelope of the requested version, including any
 * direct SOAP Fault. Set *is_fault to 0 or 1 only on success (may be NULL).
 * Returns 1, -1 invalid, -2 limit (0 selects 1 MiB), or -3 allocation.
 * This is structural validation, not application schema or WS-* validation. */
int mulib_cdd_soap_inspect(int soap12, const void *xml, size_t length,
    size_t max_length, int *is_fault);
/** Send a validated envelope and validate the resulting HTTP media type,
 * encoding, SOAP structure and HTTP status/fault pairing. 1 includes valid
 * faults. Returns inspect or MuW3 errors otherwise. max_length bounds both XML
 * documents. Action is optional HTTP-safe text (max 240 visible ASCII bytes,
 * excluding quotes and backslashes). Outputs unchanged on failure. */
int mulib_cdd_soap_call(const mulib_muw3_client_t *client, const char *url,
    int soap12, const char *action, const char *envelope_xml, size_t max_length,
    mulib_muw3_response_t *out, int *is_fault);

/* Compatibility: the old call sends raw XML and returns the raw HTTP result
 * (including non-SOAP HTTP errors), with a default 1 MiB request limit. */
char *mulib_soap_escape_text(const char *, size_t, size_t);
char *mulib_soap_envelope(int, const char *, const char *, size_t);
int mulib_soap_call(const mulib_muw3_client_t *, const char *, int, const char *,
    const char *, mulib_muw3_response_t *);
#ifdef __cplusplus
}
#endif
#endif /* declarations */

#if defined(MULIB_IMPL_CDD_SOAP) || defined(MULIB_IMPL_SOAP)
#ifndef MULIB_SOAP_IMPLEMENTED
#define MULIB_SOAP_IMPLEMENTED
#include "mulib-xml.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

#define MULIB_CDD_SOAP_LIMIT_ ((size_t)1048576)
#define MULIB_CDD_SOAP_ATTRIBUTES_ 128
#define MULIB_CDD_SOAP_NS11_ "http://schemas.xmlsoap.org/soap/envelope/"
#define MULIB_CDD_SOAP_NS12_ "http://www.w3.org/2003/05/soap-envelope"

static size_t mulib_cdd_soap_bounded_(const char *s, size_t max)
{
  size_t n = 0;
  while (n < max && s[n]) n++;
  return n;
}

static const char *mulib_cdd_soap_escape_(char c)
{
  switch (c) {
  case '&': return "&amp;";
  case '<': return "&lt;";
  case '>': return "&gt;";
  case '\'': return "&apos;";
  case '"': return "&quot;";
  case '\r': return "&#13;"; /* XML newline normalization must preserve CR */
  default: return NULL;
  }
}

char *mulib_cdd_soap_escape_text(const char *s, size_t n, size_t max)
{
  size_t bytes = 0, pos = 0;
  char *out;
  if (!max) max = MULIB_CDD_SOAP_LIMIT_;
  if (n == SIZE_MAX || n > max || !mulib_xml_valid_text(s, n)) return NULL;
  for (size_t i = 0; i < n; i++) {
    const char *replacement = mulib_cdd_soap_escape_(s[i]);
    size_t added = replacement ? strlen(replacement) : 1;
    if (added > max - bytes) return NULL;
    bytes += added;
  }
  if (bytes == SIZE_MAX || !(out = (char *)malloc(bytes + 1))) return NULL;
  for (size_t i = 0; i < n; i++) {
    const char *replacement = mulib_cdd_soap_escape_(s[i]);
    if (replacement) {
      size_t len = strlen(replacement);
      memcpy(out + pos, replacement, len);
      pos += len;
    } else {
      out[pos++] = s[i];
    }
  }
  out[pos] = 0;
  return out;
}

static int mulib_cdd_soap_space_(char c)
{ return c == ' ' || c == '\r' || c == '\n' || c == '\t'; }

static int mulib_cdd_soap_ncname_(const char *s, size_t n)
{
  if (!n) return 0;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') continue;
    if (!i || !((c >= '0' && c <= '9') || c == '.' || c == '-')) return 0;
  }
  return 1;
}

/* Resolve the QName using closest in-scope namespace declarations. */
static const char *mulib_cdd_soap_qname_(const mulib_xml_node_t *node,
    const char *s, size_t n, const char **local)
{
  const char *colon = (const char *)memchr(s, ':', n);
  size_t prefix = colon ? (size_t)(colon - s) : 0;
  size_t skip = colon ? prefix + 1 : 0;
  if (!mulib_cdd_soap_ncname_(s + skip, n - skip) ||
      (colon && !mulib_cdd_soap_ncname_(s, prefix))) return NULL;
  *local = s + skip;
  if (prefix == 3 && !memcmp(s, "xml", 3))
    return "http://www.w3.org/XML/1998/namespace";
  if (prefix == 5 && !memcmp(s, "xmlns", 5)) return NULL;
  while (node) {
    if (colon) {
      for (size_t i = 0; i < mulib_xml_attr_count(node); i++) {
        const char *a = mulib_xml_attr_name(node, i);
        if (!strncmp(a, "xmlns:", 6) && strlen(a + 6) == prefix &&
            !memcmp(a + 6, s, prefix)) {
          const char *uri = mulib_xml_attr_value(node, i);
          return *uri ? uri : NULL;
        }
      }
    } else {
      const char *uri = mulib_xml_attr(node, "xmlns");
      if (uri) return uri;
    }
    node = mulib_xml_parent(node);
  }
  return colon ? NULL : "";
}

static int mulib_cdd_soap_element_(const mulib_xml_node_t *n,
    const char *uri, const char *expected)
{
  const char *name = mulib_xml_name(n), *local, *resolved;
  if (!name) return 0;
  resolved = mulib_cdd_soap_qname_(n, name, strlen(name), &local);
  return resolved && !strcmp(resolved, uri) && !strcmp(local, expected);
}

/* Collect direct elements, skipping comments and ignorable whitespace. */
static int mulib_cdd_soap_children_(const mulib_xml_node_t *parent,
    const mulib_xml_node_t **elements, size_t capacity, size_t *used)
{
  size_t count = 0;
  for (const mulib_xml_node_t *n = mulib_xml_first(parent); n; n = mulib_xml_next(n)) {
    mulib_xml_type_t type = mulib_xml_type(n);
    if (type == MULIB_XML_ELEMENT) {
      if (elements) {
        if (count == capacity) return 0;
        elements[count] = n;
      }
      count++;
    } else if (type != MULIB_XML_COMMENT) {
      const char *s = mulib_xml_value(n);
      if (!s) return 0;
      while (mulib_cdd_soap_space_(*s)) s++;
      if (*s) return 0;
    }
  }
  if (used) *used = count;
  return 1;
}

static int mulib_cdd_soap_plain_(const mulib_xml_node_t *parent, int nonempty)
{
  int found = 0;
  for (const mulib_xml_node_t *n = mulib_xml_first(parent); n; n = mulib_xml_next(n)) {
    const char *s;
    mulib_xml_type_t type = mulib_xml_type(n);
    if (type == MULIB_XML_COMMENT) continue;
    if (type != MULIB_XML_TEXT && type != MULIB_XML_CDATA) return 0;
    s = mulib_xml_value(n);
    while (mulib_cdd_soap_space_(*s)) s++;
    if (*s) found = 1;
  }
  return found || !nonempty;
}

static int mulib_cdd_soap_lang_(const char *s)
{
  int first = 1, at_start = 1;
  for (; *s; s++) {
    if ((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') ||
        (!first && *s >= '0' && *s <= '9')) {
      at_start = 0;
    } else if (*s == '-' && !at_start) {
      first = 0;
      at_start = 1;
    } else return 0;
  }
  return !at_start;
}

/* Deliberately accept one QName text node (comments/outer whitespace are OK),
 * not arbitrary mixed XML content. */
static int mulib_cdd_soap_fault_value_(const mulib_xml_node_t *parent, int primary12)
{
  const char *uri, *local, *value = NULL;
  size_t width = 0;
  for (const mulib_xml_node_t *n = mulib_xml_first(parent); n; n = mulib_xml_next(n)) {
    const char *s;
    size_t len;
    mulib_xml_type_t type = mulib_xml_type(n);
    if (type == MULIB_XML_COMMENT) continue;
    if (type != MULIB_XML_TEXT && type != MULIB_XML_CDATA) return 0;
    s = mulib_xml_value(n);
    while (mulib_cdd_soap_space_(*s)) s++;
    if (!*s) continue;
    if (value) return 0;
    len = strlen(s);
    while (len && mulib_cdd_soap_space_(s[len - 1])) len--;
    value = s;
    width = len;
  }
  if (!value || !(uri = mulib_cdd_soap_qname_(parent, value, width, &local))) return 0;
  if (primary12) {
    static const char *const codes[] = {
      "VersionMismatch", "MustUnderstand", "DataEncodingUnknown", "Sender", "Receiver"
    };
    size_t n = width - (size_t)(local - value);
    if (strcmp(uri, MULIB_CDD_SOAP_NS12_)) return 0;
    for (size_t i = 0; i < sizeof(codes) / sizeof(*codes); i++)
      if (n == strlen(codes[i]) && !memcmp(local, codes[i], n)) return 1;
    return 0;
  }
  return *uri != '\0';
}

static int mulib_cdd_soap_fault_(const mulib_xml_node_t *fault, int soap12)
{
  const mulib_xml_node_t *f[5], *code, *pair[2];
  size_t n, pos, count;
  const char *ns = MULIB_CDD_SOAP_NS12_;
  if (!mulib_cdd_soap_children_(fault, f, 5, &n) || n < 2) return 0;
  if (!soap12) {
    if (!mulib_cdd_soap_element_(f[0], "", "faultcode") ||
        !mulib_cdd_soap_fault_value_(f[0], 0) ||
        !mulib_cdd_soap_element_(f[1], "", "faultstring") ||
        !mulib_cdd_soap_plain_(f[1], 0)) return 0;
    pos = 2;
    if (pos < n && mulib_cdd_soap_element_(f[pos], "", "faultactor")) {
      if (!mulib_cdd_soap_plain_(f[pos], 1)) return 0;
      pos++;
    }
    if (pos < n && mulib_cdd_soap_element_(f[pos], "", "detail")) pos++;
    return pos == n;
  }
  if (!mulib_cdd_soap_element_(f[0], ns, "Code") ||
      !mulib_cdd_soap_element_(f[1], ns, "Reason") ||
      !mulib_cdd_soap_children_(f[1], NULL, 0, &count) || !count) return 0;
  code = f[0];
  do {
    if (!mulib_cdd_soap_children_(code, pair, 2, &count) || !count ||
        !mulib_cdd_soap_element_(pair[0], ns, "Value") ||
        !mulib_cdd_soap_fault_value_(pair[0], code == f[0])) return 0;
    code = count == 2 ? pair[1] : NULL;
    if (code && !mulib_cdd_soap_element_(code, ns, "Subcode")) return 0;
  } while (code);
  for (const mulib_xml_node_t *node = mulib_xml_first(f[1]); node; node = mulib_xml_next(node)) {
    const char *lang;
    if (mulib_xml_type(node) != MULIB_XML_ELEMENT) continue;
    if (!mulib_cdd_soap_element_(node, ns, "Text") ||
        !(lang = mulib_xml_attr(node, "xml:lang")) || !mulib_cdd_soap_lang_(lang) ||
        !mulib_cdd_soap_plain_(node, 0)) return 0;
  }
  pos = 2;
  if (pos < n && mulib_cdd_soap_element_(f[pos], ns, "Node")) {
    if (!mulib_cdd_soap_plain_(f[pos], 1)) return 0;
    pos++;
  }
  if (pos < n && mulib_cdd_soap_element_(f[pos], ns, "Role")) {
    if (!mulib_cdd_soap_plain_(f[pos], 1)) return 0;
    pos++;
  }
  if (pos < n && mulib_cdd_soap_element_(f[pos], ns, "Detail")) pos++;
  return pos == n;
}

/* The XML DOM checks each new attribute against the existing list. Bound that
 * cost per element before parsing. This only scans quotes in markup; the XML
 * parser, not this pass, checks well-formedness. */
static int mulib_cdd_soap_attributes_(const char *p, size_t n)
{
  const char *end = p + n;
  while (p < end) {
    const char *close = NULL;
    size_t width = 0;
    unsigned attributes = 0;
    p = (const char *)memchr(p, '<', (size_t)(end - p));
    if (!p) return 1;
    if (end - p >= 4 && !memcmp(p, "<!--", 4)) {
      close = "-->";
      width = 3;
      p += 4;
    } else if (end - p >= 9 && !memcmp(p, "<![CDATA[", 9)) {
      close = "]]>";
      width = 3;
      p += 9;
    } else if (end - p >= 2 && p[1] == '?') {
      close = "?>";
      width = 2;
      p += 2;
    } else if (end - p >= 2 && p[1] == '!') {
      return 1; /* XML kernel rejects other declarations without parsing them */
    } else {
      for (++p; p < end && *p != '>'; p++) {
        if (*p == '\'' || *p == '"') {
          char quote = *p++;
          if (++attributes > MULIB_CDD_SOAP_ATTRIBUTES_) return 0;
          p = (const char *)memchr(p, quote, (size_t)(end - p));
          if (!p) return 1; /* unterminated; let XML kernel classify it */
        }
      }
      if (p < end) p++;
      continue;
    }
    while ((size_t)(end - p) >= width && memcmp(p, close, width)) p++;
    if ((size_t)(end - p) < width) return 1;
    p += width;
  }
  return 1;
}

int mulib_cdd_soap_inspect(int soap12, const void *xml, size_t n, size_t max, int *out)
{
  const char *ns = soap12 ? MULIB_CDD_SOAP_NS12_ : MULIB_CDD_SOAP_NS11_;
  mulib_xml_error_t error;
  const mulib_xml_node_t *root, *parts[2], *body, *fault = NULL;
  mulib_xml_node_t *doc;
  size_t count, body_count;
  int result = -1;
  if (!xml || !n) return -1;
  if (n > (size_t)PTRDIFF_MAX || n > (max ? max : MULIB_CDD_SOAP_LIMIT_) ||
      !mulib_cdd_soap_attributes_((const char *)xml, n)) return -2;
  doc = mulib_xml_parse((const char *)xml, n, &error);
  if (!doc) {
    if (error.message && !strcmp(error.message, "allocation failed")) return -3;
    if (error.message && !strcmp(error.message, "XML nesting limit exceeded")) return -2;
    return -1;
  }
  root = mulib_xml_root(doc);
  if (!mulib_cdd_soap_element_(root, ns, "Envelope") ||
      !mulib_cdd_soap_children_(root, parts, 2, &count) || !count) goto done;
  if (count == 2 && (!mulib_cdd_soap_element_(parts[0], ns, "Header") ||
      !mulib_cdd_soap_children_(parts[0], NULL, 0, NULL))) goto done;
  body = parts[count - 1];
  if (!mulib_cdd_soap_element_(body, ns, "Body") ||
      !mulib_cdd_soap_children_(body, NULL, 0, &body_count)) goto done;
  for (const mulib_xml_node_t *node = mulib_xml_first(body); node; node = mulib_xml_next(node)) {
    const char *qname = mulib_xml_name(node), *local, *uri;
    if (!qname) continue;
    uri = mulib_cdd_soap_qname_(node, qname, strlen(qname), &local);
    if (!uri) goto done;
    if (!strcmp(local, "Fault") && !strcmp(uri, ns)) {
      if (body_count != 1 || !mulib_cdd_soap_fault_(node, soap12)) goto done;
      fault = node;
    }
  }
  if (out) *out = fault != NULL;
  result = 1;
done:
  mulib_xml_destroy(doc);
  return result;
}

/* Parse the fragment within isolated wrappers so it cannot close one wrapper
 * and inject otherwise valid markup into the next envelope component. */
static int mulib_cdd_soap_fragment_(const char *start, const char *fragment, size_t n)
{
  static const char open[] = "<soap:fragment>";
  static const char close[] = "</soap:fragment></soap:Envelope>";
  size_t a = strlen(start), b = sizeof(open) - 1, c = sizeof(close) - 1;
  mulib_xml_node_t *doc;
  char *s;
  if (n > (size_t)PTRDIFF_MAX - a - b - c || !(s = (char *)malloc(a + b + c + n))) return 0;
  memcpy(s, start, a);
  memcpy(s + a, open, b);
  memcpy(s + a + b, fragment, n);
  memcpy(s + a + b + n, close, c);
  doc = mulib_cdd_soap_attributes_(s, a + b + n + c) ?
      mulib_xml_parse(s, a + b + n + c, NULL) : NULL;
  free(s);
  if (!doc) return 0;
  mulib_xml_destroy(doc);
  return 1;
}

char *mulib_cdd_soap_envelope(int soap12, const char *header, const char *body, size_t max)
{
  static const char start11[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<soap:Envelope xmlns:soap=\"" MULIB_CDD_SOAP_NS11_ "\">";
  static const char start12[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<soap:Envelope xmlns:soap=\"" MULIB_CDD_SOAP_NS12_ "\">";
  static const char suffix[] = "</soap:Body></soap:Envelope>";
  const char *start = soap12 ? start12 : start11;
  const char *parts[7] = {start, "<soap:Header>", header, "</soap:Header>",
    "<soap:Body>", body, suffix};
  size_t lengths[7], total = 0;
  char *out, *dest;
  if (!max) max = MULIB_CDD_SOAP_LIMIT_;
  if (!body) return NULL;
  for (size_t i = 0; i < 7; i++) {
    if (i >= 1 && i <= 3 && (!header || !*header)) {
      lengths[i] = 0;
      continue;
    }
    lengths[i] = mulib_cdd_soap_bounded_(parts[i], max - total);
    if (parts[i][lengths[i]]) return NULL;
    total += lengths[i];
  }
  if (total == SIZE_MAX || (lengths[2] && !mulib_cdd_soap_fragment_(start, header, lengths[2])) ||
      (lengths[5] && !mulib_cdd_soap_fragment_(start, body, lengths[5]))) return NULL;
  out = (char *)malloc(total + 1);
  if (!out) return NULL;
  dest = out;
  for (size_t i = 0; i < 7; i++) {
    if (!lengths[i]) continue;
    memcpy(dest, parts[i], lengths[i]);
    dest += lengths[i];
  }
  *dest = 0;
  if (mulib_cdd_soap_inspect(soap12, out, total, max, NULL) == 1) return out;
  free(out);
  return NULL;
}

static int mulib_cdd_soap_equal_(const char *s, size_t n, const char *lower)
{
  if (n != strlen(lower)) return 0;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    if (c != (unsigned char)lower[i]) return 0;
  }
  return 1;
}

static int mulib_cdd_soap_token_(unsigned char c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
      (c >= '0' && c <= '9') || (c && strchr("!#$%&'*+-.^_`|~", c));
}

static const char *mulib_cdd_soap_ows_(const char *s)
{
  while (*s == ' ' || *s == '\t') s++;
  return s;
}

static int mulib_cdd_soap_media_(const char *s, int soap12)
{
  const char *start;
  int charset = 0, action = 0;
  s = start = mulib_cdd_soap_ows_(s);
  while (*s == '/' || mulib_cdd_soap_token_((unsigned char)*s)) s++;
  if (!mulib_cdd_soap_equal_(start, (size_t)(s - start),
      soap12 ? "application/soap+xml" : "text/xml")) return 0;
  s = mulib_cdd_soap_ows_(s);
  while (*s == ';') {
    const char *value;
    size_t n, width;
    int quoted;
    s = start = mulib_cdd_soap_ows_(s + 1);
    while (mulib_cdd_soap_token_((unsigned char)*s)) s++;
    n = (size_t)(s - start);
    if (!n || *s != '=') return 0;
    s++;
    quoted = *s == '"';
    if (quoted) {
      value = ++s;
      while (*s != '"') {
        unsigned char c = (unsigned char)*s++;
        if (c == '\\') c = (unsigned char)*s++;
        if ((c < 32 && c != '\t') || c >= 127) return 0;
      }
      width = (size_t)(s++ - value);
    } else {
      value = s;
      while (mulib_cdd_soap_token_((unsigned char)*s)) s++;
      width = (size_t)(s - value);
      if (!width) return 0;
    }
    if (mulib_cdd_soap_equal_(start, n, "charset")) {
      if (charset || !mulib_cdd_soap_equal_(value, width, "utf-8")) return 0;
      charset = 1;
    } else if (mulib_cdd_soap_equal_(start, n, "action")) {
      if (!soap12 || action || !quoted || !width) return 0;
      action = 1;
    }
    s = mulib_cdd_soap_ows_(s);
  }
  return !*s;
}

static int mulib_cdd_soap_http_(const mulib_http_message_t *m, int soap12)
{
  size_t cursor = 0;
  const char *type = mulib_http_header_next(m, "Content-Type", &cursor);
  const char *encoding;
  if (!type || mulib_http_header_next(m, "Content-Type", &cursor) ||
      !mulib_cdd_soap_media_(type, soap12)) return 0;
  cursor = 0;
  encoding = mulib_http_header_next(m, "Content-Encoding", &cursor);
  return !encoding || (!mulib_http_header_next(m, "Content-Encoding", &cursor) &&
      mulib_cdd_soap_equal_(encoding, strlen(encoding), "identity"));
}

static int mulib_cdd_soap_post_(const mulib_muw3_client_t *client, const char *url,
    int soap12, const char *action, const char *xml, size_t n,
    mulib_muw3_response_t *out, int checked)
{
  mulib_http_header_t headers[3];
  mulib_muw3_request_t q;
  char type[512], action11[256];
  size_t count = 0;
  if (!url || !out) return -1;
  if (action) {
    size_t a = mulib_cdd_soap_bounded_(action, 240);
    if (action[a] || (checked && soap12 && !a)) return -1;
    for (size_t i = 0; i < a; i++) {
      unsigned char c = (unsigned char)action[i];
      if (c < 33 || c >= 127 || c == '"' || c == '\\') return -1;
    }
  }
  if (soap12 && action)
    (void)snprintf(type, sizeof(type), "application/soap+xml; charset=utf-8; action=\"%s\"", action);
  else
    strcpy(type, soap12 ? "application/soap+xml; charset=utf-8" : "text/xml; charset=utf-8");
  headers[count].name = (char *)"Content-Type";
  headers[count++].value = type;
  headers[count].name = (char *)"Accept-Encoding";
  headers[count++].value = (char *)"identity";
  if (!soap12 && (action || checked)) {
    (void)snprintf(action11, sizeof(action11), "\"%s\"", action ? action : "");
    headers[count].name = (char *)"SOAPAction";
    headers[count++].value = action11;
  }
  q.url = url;
  q.method = "POST";
  q.headers = headers;
  q.header_count = count;
  q.body = xml;
  q.body_length = n;
  return (int)mulib_muw3_perform(client, &q, out);
}

int mulib_cdd_soap_call(const mulib_muw3_client_t *client, const char *url,
    int soap12, const char *action, const char *xml, size_t limit,
    mulib_muw3_response_t *out, int *is_fault)
{
  mulib_muw3_response_t response;
  mulib_muw3_client_t bounded;
  size_t n, max = limit ? limit : MULIB_CDD_SOAP_LIMIT_;
  size_t http_limit;
  int rc, fault;
  if (!xml || !url || !out) return -1;
  n = mulib_cdd_soap_bounded_(xml, max);
  if (xml[n]) return -2;
  bounded = client ? *client : mulib_muw3_client_default();
  http_limit = bounded.options.web.http.max_body_bytes;
  if (!http_limit) http_limit = 16777216; /* HTTP default */
  if (n > http_limit) return -2;
  if (max < http_limit) bounded.options.web.http.max_body_bytes = max;
  rc = mulib_cdd_soap_inspect(soap12, xml, n, max, NULL);
  if (rc != 1) return rc;
  memset(&response, 0, sizeof(response));
  rc = mulib_cdd_soap_post_(&bounded, url, soap12, action, xml, n, &response, 1);
  if (rc != 1) return rc;
  if (!mulib_cdd_soap_http_(&response.message, soap12)) rc = -1;
  else rc = mulib_cdd_soap_inspect(soap12, response.message.body,
      response.message.body_length, max, &fault);
  if (rc == 1) {
    unsigned status = response.message.status;
    if ((!fault && status >= 200 && status < 300) ||
        (fault && (status == 500 || (soap12 && status == 400)))) {
      *out = response;
      if (is_fault) *is_fault = fault;
      return 1;
    }
    rc = -1;
  }
  mulib_muw3_response_free(&response);
  return rc;
}

char *mulib_soap_escape_text(const char *s, size_t length, size_t max)
{ return mulib_cdd_soap_escape_text(s, length, max); }

char *mulib_soap_envelope(int soap12, const char *header, const char *body, size_t max)
{ return mulib_cdd_soap_envelope(soap12, header, body, max); }

int mulib_soap_call(const mulib_muw3_client_t *client, const char *url,
    int soap12, const char *action, const char *xml, mulib_muw3_response_t *out)
{
  size_t n;
  if (!xml) return -1;
  n = mulib_cdd_soap_bounded_(xml, MULIB_CDD_SOAP_LIMIT_);
  if (xml[n]) return -2;
  return mulib_cdd_soap_post_(client, url, soap12, action, xml, n, out, 0);
}

#undef MULIB_CDD_SOAP_LIMIT_
#undef MULIB_CDD_SOAP_NS11_
#undef MULIB_CDD_SOAP_NS12_
#ifdef __cplusplus
}
#endif
#endif /* implementation guard */
#endif /* implementation switch */
