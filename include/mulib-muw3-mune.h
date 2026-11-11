/** @file mulib-muw3-mune.h
 * @brief Embeddable Mune Weblet engine with lmuw3, JSON/YAML and MicroChan.
 * Define MULIB_IMPL_MUW3_MUNE and all transitive switches; see the manual. */
#ifndef MULIB_MUW3_MUNE_H
#define MULIB_MUW3_MUNE_H
#include "mulib-muw3.h"
#include "mulib-mune.h"
#include "mulib-microchan.h"
#include "mulib-yaml.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_muw3_engine mulib_muw3_engine_t;
typedef struct mulib_muw3_engine_options {
  mulib_mune_runtime_options_t lua;
  size_t document_bytes, max_channels; /* zero: 1 MiB, 16 connections */
} mulib_muw3_engine_options_t;
/** Copy client configuration (NULL selects defaults), create a persistent Mune
 * state and install require('lmuw3') and global muw3. Callback state is borrowed.
 * NULL on failure; diagnostics use static text on creation failure. */
mulib_muw3_engine_t *mulib_muw3_engine_create (const mulib_muw3_client_t *,
    const mulib_muw3_engine_options_t *, mulib_mune_error_t *);
/** Destroy only while idle, after freeing all Mune results. Closes IPC channels. */
void mulib_muw3_engine_destroy (mulib_muw3_engine_t *);
/** Borrow state for application bindings. No concurrent/reentrant execution. */
mulib_mune_state_t *mulib_muw3_engine_lua (mulib_muw3_engine_t *);
/** Persistent execution; result must point to NULL. Mune ownership/error rules.
 * Side effects (including requests already sent) persist after script errors. */
mulib_mune_status_t mulib_muw3_engine_run (mulib_muw3_engine_t *, const char *, size_t,
    mulib_mune_result_t **, mulib_mune_error_t *);
mulib_mune_status_t mulib_muw3_engine_run_file (mulib_muw3_engine_t *, const char *,
    mulib_mune_result_t **, mulib_mune_error_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_MUW3_MUNE) && !defined(MULIB_MUW3_MUNE_IMPLEMENTED)
#define MULIB_MUW3_MUNE_IMPLEMENTED
#include "detail/mulib-lmuw3.inc"
#include <stdio.h>
typedef struct mulib_muw3_channel_ { mulib_microchan_client_t *client; int64_t id; } mulib_muw3_channel_;
struct mulib_muw3_engine {
  mulib_muw3_client_t client;
  mulib_muw3_engine_options_t options;
  mulib_mune_state_t *lua;
  mulib_muw3_channel_ *channels;
  int64_t next_channel;
  int running;
};
static const char *mulib_muw3_string_ (mulib_mune_value_t v, size_t *length, int text)
{
  size_t n;
  const char *s = mulib_mune_value_string (v, &n);
  if (!s || (text && memchr (s, 0, n))) return NULL;
  if (length) *length = n;
  return s;
}
static int mulib_muw3_lstring_ (mulib_mune_state_t *s, const void *text, size_t n,
    mulib_mune_result_t *roots, mulib_mune_value_t *out)
{
  return !mulib_mune_make_string (s, (const char *)text, n, out) && mulib_mune_result_push (roots, *out);
}
static int mulib_muw3_ltable_ (mulib_mune_state_t *s, mulib_mune_result_t *roots, mulib_mune_value_t *out)
{ return !mulib_mune_make_table (s, out) && mulib_mune_result_push (roots, *out); }
static int mulib_muw3_lfield_ (mulib_mune_state_t *s, mulib_mune_value_t table,
    const char *name, mulib_mune_value_t v, mulib_mune_result_t *roots)
{
  mulib_mune_value_t key;
  return mulib_muw3_lstring_ (s, name, strlen (name), roots, &key) && !mulib_mune_raw_set (s, table, key, v);
}
static int mulib_muw3_text_field_ (mulib_mune_state_t *s, mulib_mune_value_t table,
    const char *name, const void *text, size_t n, mulib_mune_result_t *roots)
{
  mulib_mune_value_t v;
  return mulib_muw3_lstring_ (s, text, n, roots, &v) && mulib_muw3_lfield_ (s, table, name, v, roots);
}
static int mulib_muw3_fields_ (mulib_mune_state_t *s, mulib_mune_value_t table,
    mulib_http_header_t fields[256], size_t *count)
{
  size_t cursor = 0, n = 0;
  mulib_mune_value_t k, v, a, b;
  int rc;
  if (table.type != MULIB_MUNE_TABLE) return 0;
  while ((rc = mulib_mune_table_next (s, table, &cursor, &k, &v)) == 1) {
    if (++n > 256) return 0;
  }
  if (rc < 0) return 0;
  for (size_t i = 0; i < n; ++i) {
    if (mulib_mune_raw_get (s, table, mulib_mune_integer ((int64_t)i + 1), &v)
        || v.type != MULIB_MUNE_TABLE
        || mulib_mune_raw_get (s, v, mulib_mune_integer (1), &a)
        || mulib_mune_raw_get (s, v, mulib_mune_integer (2), &b)) return 0;
    fields[i].name = (char *)mulib_muw3_string_ (a, NULL, 1);
    fields[i].value = (char *)mulib_muw3_string_ (b, NULL, 1);
    if (!fields[i].name || !fields[i].value) return 0;
  }
  *count = n; return 1;
}
static int mulib_muw3_lheaders_ (mulib_mune_state_t *s, mulib_mune_value_t table, const char *name,
    const mulib_http_header_t *fields, size_t count, mulib_mune_result_t *roots)
{
  mulib_mune_value_t list, pair, text;
  if (!mulib_muw3_ltable_ (s, roots, &list)) return 0;
  for (size_t i = 0; i < count; ++i) {
    if (!mulib_muw3_ltable_ (s, roots, &pair)
        || !mulib_muw3_lstring_ (s, fields[i].name, strlen (fields[i].name), roots, &text)
        || mulib_mune_raw_set (s, pair, mulib_mune_integer (1), text)
        || !mulib_muw3_lstring_ (s, fields[i].value, strlen (fields[i].value), roots, &text)
        || mulib_mune_raw_set (s, pair, mulib_mune_integer (2), text)
        || mulib_mune_raw_set (s, list, mulib_mune_integer ((int64_t)i + 1), pair)) return 0;
  }
  return mulib_muw3_lfield_ (s, table, name, list, roots);
}
static int mulib_muw3_ljson_ (mulib_mune_state_t *s, const mulib_json_t *json,
    mulib_mune_result_t *roots, mulib_mune_value_t *out, unsigned depth)
{
  mulib_mune_value_t v = mulib_mune_nil ();
  const char *text;
  size_t n;
  if (depth > 64) return 0;
  switch (mulib_json_typeof (json)) {
  case MULIB_JSON_NULL:
    if (mulib_mune_get_global (s, "__mulib_muw3_null", &v)) return 0;
    break;
  case MULIB_JSON_BOOL: v = mulib_mune_boolean (mulib_json_boolean (json)); break;
  case MULIB_JSON_NUMBER: v = mulib_mune_number (mulib_json_numeric (json)); break;
  case MULIB_JSON_STRING:
    text = mulib_json_text (json, &n);
    if (!mulib_muw3_lstring_ (s, text, n, roots, &v)) return 0;
    break;
  case MULIB_JSON_ARRAY: case MULIB_JSON_OBJECT:
    if (!mulib_muw3_ltable_ (s, roots, &v)) return 0;
    for (size_t i = 0; i < mulib_json_size (json); ++i) {
      mulib_mune_value_t key, child;
      if (mulib_json_typeof (json) == MULIB_JSON_ARRAY) key = mulib_mune_integer ((int64_t)i + 1);
      else {
        text = mulib_json_key (json, i, &n);
        if (!mulib_muw3_lstring_ (s, text, n, roots, &key)) return 0;
      }
      if (!mulib_muw3_ljson_ (s, mulib_json_at (json, i), roots, &child, depth + 1)
          || mulib_mune_raw_set (s, v, key, child)) return 0;
    }
    if (mulib_json_typeof (json) == MULIB_JSON_ARRAY) {
      mulib_mune_value_t mt;
      if (mulib_mune_get_global (s, "__mulib_muw3_array", &mt) || mulib_mune_set_metatable (s, v, mt)) return 0;
    }
    break;
  }
  if (!mulib_mune_result_push (roots, v)) return 0;
  *out = v; return 1;
}
static mulib_mune_status_t mulib_muw3_native_error_ (mulib_mune_state_t *s,
    mulib_mune_result_t *out, int code, const char *text)
{
  mulib_mune_value_t message;
  return mulib_mune_result_push (out, mulib_mune_nil ())
      && !mulib_mune_make_string (s, text, strlen (text), &message)
      && mulib_mune_result_push (out, message)
      && mulib_mune_result_push (out, mulib_mune_integer (code)) ? MULIB_MUNE_OK : MULIB_MUNE_MEMORY;
}
static mulib_mune_status_t mulib_muw3_native_ (mulib_mune_state_t *s,
    const mulib_mune_value_t *a, size_t argc, mulib_mune_result_t *out, void *user)
{
  mulib_muw3_engine_t *e = (mulib_muw3_engine_t *)user;
  mulib_mune_result_t *roots;
  mulib_mune_value_t v = mulib_mune_boolean (1);
  const char *op, *error = "invalid lmuw3 arguments";
  int code = -1, ok = 1;
  if (!argc || !(op = mulib_muw3_string_ (a[0], NULL, 1))) return MULIB_MUNE_INVALID;
  roots = mulib_mune_result_create (s);
  if (!roots) return MULIB_MUNE_MEMORY;
  if (!strcmp (op, "request") && argc == 5) {
    mulib_http_header_t headers[256];
    mulib_muw3_request_t request = {0};
    mulib_muw3_response_t response = {0};
    request.method = mulib_muw3_string_ (a[1], NULL, 1);
    request.url = mulib_muw3_string_ (a[2], NULL, 1);
    request.body = mulib_muw3_string_ (a[4], &request.body_length, 0);
    request.headers = headers;
    if (request.method && request.url && request.body
        && mulib_muw3_fields_ (s, a[3], headers, &request.header_count)) {
      code = mulib_muw3_perform (&e->client, &request, &response);
      error = mulib_muw3_strerror (code);
      if (code == MULIB_MUW3_OK) {
        code = 0;
        ok = mulib_muw3_ltable_ (s, roots, &v)
            && mulib_muw3_lfield_ (s, v, "status", mulib_mune_integer (response.message.status), roots)
            && mulib_muw3_lfield_ (s, v, "redirects", mulib_mune_integer (response.redirects), roots)
            && mulib_muw3_text_field_ (s, v, "url", response.url, strlen (response.url), roots)
            && mulib_muw3_text_field_ (s, v, "body", response.message.body, response.message.body_length, roots)
            && mulib_muw3_lheaders_ (s, v, "headers", response.message.headers, response.message.header_count, roots)
            && mulib_muw3_lheaders_ (s, v, "trailers", response.message.trailers, response.message.trailer_count, roots);
      }
    }
    mulib_muw3_response_free (&response);
  } else if ((!strcmp (op, "json_decode") || !strcmp (op, "yaml_decode")
      || !strcmp (op, "json_encode") || !strcmp (op, "yaml_encode")) && argc == 2) {
    size_t n;
    const char *text = mulib_muw3_string_ (a[1], &n, 0);
    if (text && n <= e->options.document_bytes) {
      mulib_json_t *json = !strcmp (op, "yaml_decode") ? mulib_yaml_parse (text, n, NULL) : mulib_json_parse (text, n, NULL);
      if (json) {
        code = 0;
        if (!strcmp (op, "json_decode") || !strcmp (op, "yaml_decode")) ok = mulib_muw3_ljson_ (s, json, roots, &v, 0);
        else {
          char *encoded = !strcmp (op, "yaml_encode") ? mulib_yaml_stringify (json, NULL) : mulib_json_stringify (json, 0);
          if (!encoded) ok = 0;
          else if (strlen (encoded) > e->options.document_bytes) { code = -2; error = "document limit"; }
          else ok = mulib_muw3_lstring_ (s, encoded, strlen (encoded), roots, &v);
          free (encoded);
        }
        mulib_json_destroy (json);
      } else error = "invalid data document";
    } else { code = -2; error = "document limit or non-string input"; }
  } else if (!strcmp (op, "form") && argc == 2) {
    mulib_http_header_t headers[256]; size_t count;
    char *text = NULL;
    if (mulib_muw3_fields_ (s, a[1], headers, &count)) {
      code = mulib_muw3_form (headers, count, e->options.document_bytes, &text);
      error = mulib_muw3_strerror (code);
      if (code == 1) { code = 0; ok = mulib_muw3_lstring_ (s, text, strlen (text), roots, &v); }
    }
    free (text);
  } else if (!strcmp (op, "resolve") && argc == 3) {
    const char *base = mulib_muw3_string_ (a[1], NULL, 1), *ref = mulib_muw3_string_ (a[2], NULL, 1);
    mulib_uri_t b, r;
    if (base && ref && strlen (base) <= e->options.document_bytes && strlen (ref) <= e->options.document_bytes
        && mulib_uri_parse (base, strlen (base), &b) && mulib_uri_parse (ref, strlen (ref), &r)) {
      char *text = mulib_uri_resolve (&b, &r);
      code = 0; ok = text && mulib_muw3_lstring_ (s, text, strlen (text), roots, &v); free (text);
    }
  } else if (!strcmp (op, "open") && argc == 2) {
    const char *path = mulib_muw3_string_ (a[1], NULL, 1);
    size_t i;
    for (i = 0; i < e->options.max_channels && e->channels[i].client; ++i) {}
    if (i == e->options.max_channels || e->next_channel == INT64_MAX) { code = MULIB_CT_ELIMIT; error = "channel limit"; }
    else if (path) {
      code = mulib_microchan_open (&e->channels[i].client, path, e->options.document_bytes, e->client.options.timeout_ms);
      error = mulib_ct_strerror (code);
      if (!code) { e->channels[i].id = ++e->next_channel; v = mulib_mune_integer (e->next_channel); }
    }
  } else if (argc >= 2 && a[1].type == MULIB_MUNE_INTEGER) {
    mulib_muw3_channel_ *channel = NULL;
    for (size_t i = 0; i < e->options.max_channels; ++i)
      if (e->channels[i].client && e->channels[i].id == a[1].as.integer) channel = e->channels + i;
    if (channel) {
      if (!strcmp (op, "close") && argc == 2) {
        mulib_microchan_close (channel->client); channel->client = NULL; code = 0;
      } else if (!strcmp (op, "send") && argc == 6 && a[2].type == MULIB_MUNE_INTEGER
          && a[3].type == MULIB_MUNE_INTEGER && a[3].as.integer > 0
          && a[2].as.integer >= MULIB_MICROCHAN_SUBSCRIBE && a[2].as.integer <= MULIB_MICROCHAN_PUBLISH) {
        mulib_microchan_message_t m = {0};
        size_t n;
        const char *topic = mulib_muw3_string_ (a[4], &n, 1);
        m.data = (const unsigned char *)mulib_muw3_string_ (a[5], &m.length, 0);
        if (topic && n < sizeof m.topic && m.data) {
          memcpy (m.topic, topic, n + 1); m.opcode = (unsigned)a[2].as.integer; m.id = (uint64_t)a[3].as.integer;
          code = mulib_microchan_send (channel->client, &m); error = mulib_ct_strerror (code);
        }
      } else if (!strcmp (op, "receive") && argc == 3 && a[2].type == MULIB_MUNE_INTEGER
          && a[2].as.integer >= 0 && a[2].as.integer <= 3600000) {
        mulib_microchan_message_t m;
        code = mulib_microchan_receive (channel->client, (unsigned)a[2].as.integer, &m);
        error = mulib_ct_strerror (code);
        if (code == MULIB_CT_AGAIN) { code = 0; v = mulib_mune_nil (); }
        else if (!code) {
          if (m.id > INT64_MAX) { code = MULIB_CT_ELIMIT; error = "correlation ID exceeds Lua integer"; }
          else ok = mulib_muw3_ltable_ (s, roots, &v)
              && mulib_muw3_lfield_ (s, v, "id", mulib_mune_integer ((int64_t)m.id), roots)
              && mulib_muw3_lfield_ (s, v, "opcode", mulib_mune_integer (m.opcode), roots)
              && mulib_muw3_lfield_ (s, v, "status", mulib_mune_integer (m.status), roots)
              && mulib_muw3_text_field_ (s, v, "topic", m.topic, strlen (m.topic), roots)
              && mulib_muw3_text_field_ (s, v, "body", m.data, m.length, roots);
        }
      }
    } else error = "closed or unknown channel";
  }
  {
    mulib_mune_status_t status = MULIB_MUNE_MEMORY;
    if (ok) status = code ? mulib_muw3_native_error_ (s, out, code, error)
        : mulib_mune_result_push (out, v) ? MULIB_MUNE_OK : MULIB_MUNE_MEMORY;
    mulib_mune_result_destroy (roots); return status;
  }
}
void mulib_muw3_engine_destroy (mulib_muw3_engine_t *e)
{
  if (!e || e->running) return;
  mulib_mune_state_destroy (e->lua);
  if (e->channels) for (size_t i = 0; i < e->options.max_channels; ++i) mulib_microchan_close (e->channels[i].client);
  free (e->channels); free (e);
}
mulib_muw3_engine_t *mulib_muw3_engine_create (const mulib_muw3_client_t *client,
    const mulib_muw3_engine_options_t *options, mulib_mune_error_t *error)
{
  mulib_muw3_engine_t *e = (mulib_muw3_engine_t *)calloc (1, sizeof *e);
  mulib_strbuf_t source = {0};
  mulib_mune_result_t *result = NULL;
  int rc = MULIB_MUNE_MEMORY;
  if (!e) goto fail;
  e->client = client ? *client : mulib_muw3_client_default ();
  if (options) e->options = *options;
  if (!e->options.document_bytes) e->options.document_bytes = 1048576;
  if (!e->options.max_channels) e->options.max_channels = 16;
  if (e->options.document_bytes > 16777216 || e->options.max_channels > 256) { rc = MULIB_MUNE_INVALID; goto fail; }
  e->channels = (mulib_muw3_channel_ *)calloc (e->options.max_channels, sizeof (*e->channels));
  e->lua = mulib_mune_state_create (&e->options.lua);
  if (!e->channels || !e->lua) goto fail;
  for (size_t i = 0; i < sizeof mulib_muw3_lua_source_ / sizeof *mulib_muw3_lua_source_; ++i)
    if (!mulib_strbuf_append (&source, mulib_muw3_lua_source_[i])) goto fail;
  rc = mulib_mune_bind (e->lua, "__mulib_muw3", mulib_muw3_native_, e);
  if (!rc) rc = mulib_mune_run_source (e->lua, source.data, source.length, &result, error);
  if (!rc) rc = mulib_mune_set_global (e->lua, "muw3", mulib_mune_result_get (result, 0));
  if (rc) goto fail;
  mulib_mune_result_destroy (result); mulib_strbuf_free (&source);
  if (error) { error->code = MULIB_MUNE_OK; error->offset = 0; error->message = ""; }
  return e;
fail:
  mulib_mune_result_destroy (result); mulib_strbuf_free (&source); mulib_muw3_engine_destroy (e);
  if (error) { error->code = (mulib_mune_status_t)rc; error->offset = 0; error->message = "MuW3 engine initialization failed"; }
  return NULL;
}
mulib_mune_state_t *mulib_muw3_engine_lua (mulib_muw3_engine_t *e) { return e ? e->lua : NULL; }
mulib_mune_status_t mulib_muw3_engine_run (mulib_muw3_engine_t *e, const char *source, size_t n,
    mulib_mune_result_t **result, mulib_mune_error_t *error)
{
  mulib_mune_status_t rc;
  if (!e || e->running || !result || *result || (!source && n)) rc = MULIB_MUNE_INVALID;
  else if (n > e->options.document_bytes) rc = MULIB_MUNE_LIMIT;
  else { e->running = 1; rc = mulib_mune_run_source (e->lua, source, n, result, error); e->running = 0; return rc; }
  if (error) { error->code = rc; error->offset = 0; error->message = "invalid or oversized Weblet"; }
  return rc;
}
mulib_mune_status_t mulib_muw3_engine_run_file (mulib_muw3_engine_t *e, const char *path,
    mulib_mune_result_t **result, mulib_mune_error_t *error)
{
  mulib_mune_chunk_t *chunk;
  mulib_mune_error_t local;
  mulib_mune_parse_options_t options = {0};
  mulib_mune_status_t rc;
  if (!e || !path || !result || *result || e->running) {
    if (error) { error->code = MULIB_MUNE_INVALID; error->offset = 0; error->message = "invalid Weblet file call"; }
    return MULIB_MUNE_INVALID;
  }
  options.source_bytes = e->options.document_bytes;
  chunk = mulib_mune_parse_file (path, &options, error ? error : &local);
  if (!chunk) return error ? error->code : local.code;
  e->running = 1;
  rc = mulib_mune_run (e->lua, chunk, result, error);
  e->running = 0;
  mulib_mune_chunk_destroy (chunk);
  return rc;
}
#endif
