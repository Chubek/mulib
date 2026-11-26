#include "live.h"
#include "runtime.h"
#include "on1x_runtime.h"

#include <stdlib.h>
#include <string.h>

enum {
  digits = 6,
  fixed_size = 1 + 3 * digits + 4,
  metadata_capacity = fixed_size + 4 * digits + MULIB_ELVI_DOCUMENT_MAX_URL +
                      MULIB_ELVI_DOCUMENT_MAX_CONTENT_TYPE + 9 + 16
};

static size_t bounded_length(const char *s, size_t cap)
{
  size_t n = 0;
  if (!s) return cap + 1;
  while (n <= cap && s[n]) ++n;
  return n;
}

static void decimal(char *out, size_t n)
{
  for (size_t i = digits; i; --i) {
    out[i - 1] = (char)('0' + n % 10);
    n /= 10;
  }
}

static int field(char *wire, size_t *pos, const char *s, size_t capacity)
{
  const size_t length = bounded_length(s, capacity - 1);
  if (length >= capacity || *pos > metadata_capacity - digits ||
      length > metadata_capacity - digits - *pos)
    return 0;
  decimal(wire + *pos, length);
  *pos += digits;
  memcpy(wire + *pos, s, length);
  *pos += length;
  return 1;
}

static const char *capture_error(mulib_elvi_script_capture_result_t code)
{
  switch (code) {
  case MULIB_ELVI_CAPTURE_INVALID: return "EI";
  case MULIB_ELVI_CAPTURE_DENIED: return "ED";
  case MULIB_ELVI_CAPTURE_CREDENTIALS: return "EC";
  case MULIB_ELVI_CAPTURE_TLS: return "ET";
  case MULIB_ELVI_CAPTURE_TRANSPORT: return "EN";
  case MULIB_ELVI_CAPTURE_RESPONSE: return "ER";
  case MULIB_ELVI_CAPTURE_REDIRECT: return "EM";
  case MULIB_ELVI_CAPTURE_LIMIT: return "EL";
  case MULIB_ELVI_CAPTURE_NOMEM: return "EA";
  default: return "EP";
  }
}

const char *on1x_elvi_live_snapshot_v1(const char *source, const char *url,
                                       int64_t limit)
{
  mulib_elvi_scrape_program_t program;
  mulib_elvi_script_document_t document = {0};
  on1x_elvi_live_grant_v1 grant = {0};
  const size_t source_size = bounded_length(source, ON1X_ELVI_SOURCE_LIMIT);
  const size_t url_size = bounded_length(url, MULIB_ELVI_DOCUMENT_MAX_URL - 1);
  const char *reply = "EP";
  char *wire;
  size_t used = fixed_size;

  if (!source_size || source_size > ON1X_ELVI_SOURCE_LIMIT ||
      url_size >= MULIB_ELVI_DOCUMENT_MAX_URL ||
      limit < 1 || limit > ON1X_ELVI_LIVE_MAX_BODY ||
      !on1x_elvi_compile(source, source_size, &program, NULL) ||
      program.mode != MULIB_ELVI_MODE_DOCUMENT ||
      program.artifacts.kind != MULIB_ELVI_STAGE_CAPTURE)
    return "EI";
  if (!url_size) url = program.home;
  if (!mulib_elvi_script_url_allowed(&program, url))
    return "ED";
  const on1x_elvi_live_request_v1 request = {url, program.host, (size_t)limit};
  if (on1x_elvi_live_authorize_v1(&request, &grant) != 1 || !grant.https ||
      !grant.approved_host)
    return "ED";
  const mulib_elvi_script_capture_config_t config = {
    .url = url, .approved_host = grant.approved_host, .https = grant.https,
    .max_body_bytes = (size_t)limit, .approved_home = grant.approved_home,
    .approved_cookie_path = grant.approved_cookie_path
  };
  mulib_elvi_script_capture_result_t rc = on1x_elvi_capture_https(&program, &config,
                                                                  &document);
  if (rc != MULIB_ELVI_CAPTURE_OK) return capture_error(rc);
  if (document.body_length > (size_t)limit) goto done;
  wire = malloc(metadata_capacity + document.body_length * 2 + 1);
  if (!wire) {
    reply = "EA";
    goto done;
  }
  const mulib_elvi_script_document_metadata_t *m = &document.metadata;
  wire[0] = 'O';
  decimal(wire + 1, m->status);
  decimal(wire + 1 + digits, m->tls_version);
  decimal(wire + 1 + 2 * digits, document.body_length);
  wire[fixed_size - 4] = m->certificate_verified ? '1' : '0';
  wire[fixed_size - 3] = m->hostname_verified ? '1' : '0';
  wire[fixed_size - 2] = m->chunked ? '1' : '0';
  wire[fixed_size - 1] = m->close_delimited ? '1' : '0';
  if (field(wire, &used, m->requested_url, sizeof m->requested_url) &&
      field(wire, &used, m->content_type, sizeof m->content_type) &&
      field(wire, &used, m->http_protocol, sizeof m->http_protocol) &&
      field(wire, &used, m->alpn, sizeof m->alpn) &&
      !strcmp(m->requested_url, m->effective_url) &&
      on1x_elvi_document_copy_hex(&document, wire + used, document.body_length * 2 + 1)) {
    used += document.body_length * 2;
    mulib_elvi_script_document_destroy(&document);
    reply = on1x_text_slice(wire, 0, (int64_t)used);
  }
  free(wire);
done:
  mulib_elvi_script_document_destroy(&document);
  return reply;
}
