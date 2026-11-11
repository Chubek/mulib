#include "fixtures/openai-setup.h"
#include <assert.h>
#include <stdio.h>

typedef struct fixture {
  mulib_openai_server_t server;
  mulib_webclient_receive_fn receive;
  void *receiver;
  unsigned calls, events, items, tools;
  int mode, writing, cancel;
} fixture;

static int sink (void *user, const void *bytes, size_t n)
{
  fixture *f = (fixture *)user;
  const unsigned char *p = (const unsigned char *)bytes;
  /* Every byte boundary, including HTTP chunks, SSE UTF-8 and CRLF. */
  f->writing = 1;
  for (size_t i = 0; i < n; ++i) if (!f->receive (f->receiver, p + i, 1)) { f->writing = 0; return 0; }
  f->writing = 0; return 1;
}
static mulib_json_t *json (const char *text)
{
  mulib_json_t *v = mulib_json_parse (text, strlen (text), NULL);
  assert (v); return v;
}
static int send_json (mulib_openai_writer_t *w, const char *text)
{
  mulib_json_t *v = json (text);
  int rc = mulib_openai_server_json (w, 200, v); mulib_json_destroy (v); return rc;
}
static int handler (void *user, const mulib_openai_server_request_t *q, mulib_openai_writer_t *w)
{
  fixture *f = (fixture *)user;
  ++f->calls;
  assert (!strcmp (mulib_http_header (q->http, "Authorization"), "Bearer synthetic-key"));
  assert (!strcmp (mulib_http_header (q->http, "OpenAI-Project"), "synthetic-project"));
  if (!strcmp (q->operation->id, "createResponse")) {
    assert (q->json);
    if (f->mode == 1) {
      mulib_json_t *v = json ("{\"type\":\"response.output_text.delta\",\"delta\":\"h\u00e9llo\"}");
      int rc = mulib_openai_server_begin (w, 200, "text/event-stream", NULL, 0);
      if (rc == 1) rc = mulib_openai_server_event (w, "response.output_text.delta", "1", v);
      mulib_json_destroy (v);
      if (rc != 1) return rc;
      assert (f->events == 1); /* Live delivery before handler/transport return. */
      v = json ("{\"type\":\"response.completed\",\"response\":{\"id\":\"resp_1\"}}");
      rc = mulib_openai_server_event (w, "response.completed", NULL, v);
      mulib_json_destroy (v);
      return rc == 1 ? mulib_openai_server_done (w, 0) : rc;
    }
    if (f->mode == 2) return mulib_openai_server_error (w, 429, "Try later", "rate_limit_error", NULL, "rate_limit_exceeded");
    if (f->mode == 3 && !mulib_json_get (q->json, "previous_response_id"))
      return send_json (w, "{\"id\":\"resp_tool\",\"output\":[{\"type\":\"function_call\",\"name\":\"add\",\"call_id\":\"call_1\",\"arguments\":\"{\\\"x\\\":2}\"}]}");
    if (f->mode == 3) {
      assert (!strcmp (mulib_json_text (mulib_json_get (q->json, "previous_response_id"), NULL), "resp_tool"));
      assert (!strcmp (mulib_json_text (mulib_json_get (mulib_json_at (mulib_json_get (q->json, "input"), 0), "output"), NULL), "3"));
    }
    return send_json (w, "{\"id\":\"resp_final\",\"output\":[{\"type\":\"message\",\"content\":[{\"type\":\"output_text\",\"text\":\"hello\"},{\"type\":\"output_text\",\"text\":\" world\"}]}]}");
  }
  if (!strcmp (q->operation->id, "listFiles")) {
    char *after = NULL;
    if (mulib_openai_query (q->target, "after", &after) == MULIB_OPENAI_NOT_FOUND)
      return send_json (w, "{\"data\":[{\"id\":\"file_a\"}],\"has_more\":true,\"last_id\":\"file_a\"}");
    assert (!strcmp (after, "file_a")); free (after);
    return send_json (w, "{\"data\":[{\"id\":\"file_b\"}],\"has_more\":false,\"last_id\":\"file_b\"}");
  }
  if (!strcmp (q->operation->id, "createFile")) {
    mulib_multipart_part_t p = {0}; size_t cursor = 0;
    assert (mulib_openai_server_part (q, &cursor, &p) == 1);
    assert (!strcmp (p.name, "purpose") && p.length == 10 && !memcmp (p.data, "assistants", 10));
    mulib_multipart_part_free (&p);
    assert (mulib_openai_server_part (q, &cursor, &p) == 1);
    assert (!strcmp (p.filename, "sample.bin") && p.length == 4 && !memcmp (p.data, "a\0bc", 4));
    mulib_multipart_part_free (&p);
    assert (mulib_openai_server_part (q, &cursor, &p) == 0);
    return send_json (w, "{\"id\":\"file_1\",\"object\":\"file\"}");
  }
  if (!strcmp (q->operation->id, "downloadFile")) {
    char *id = NULL;
    assert (mulib_openai_path_parameter (q->operation, q->target, "file_id", &id) == 1);
    assert (!strcmp (id, "x/y ?#")); free (id);
    return mulib_openai_server_reply (w, 200, "application/octet-stream", NULL, 0, "a\0bc", 4);
  }
  return 0;
}
static int transport (void *user, const mulib_uri_t *uri, const void *bytes, size_t n,
    mulib_webclient_receive_fn receive, void *receiver)
{
  fixture *f = (fixture *)user;
  size_t consumed = 0;
  int rc;
  assert (uri->host.length == 7 && !memcmp (uri->host.data, "fixture", 7));
  f->receive = receive; f->receiver = receiver;
  rc = mulib_openai_server_process (&f->server, bytes, n, &consumed, sink, f);
  assert (consumed == n);
  return rc == 1;
}
static int event (void *user, const mulib_openai_event_t *e)
{
  fixture *f = (fixture *)user;
  assert (f->writing && e->json);
  ++f->events;
  if (f->events == 1) assert (!strcmp (mulib_json_text (mulib_json_get (e->json, "delta"), NULL), "h\u00e9llo"));
  else assert (e->done);
  return !f->cancel;
}
static int item (void *user, const mulib_json_t *v)
{ fixture *f = (fixture *)user; assert (mulib_json_get (v, "id")); ++f->items; return 1; }
static int tool (void *user, const mulib_json_t *call, const mulib_json_t *args, mulib_json_t **out)
{
  fixture *f = (fixture *)user;
  assert (!strcmp (mulib_json_text (mulib_json_get (call, "name"), NULL), "add"));
  assert (mulib_json_numeric (mulib_json_get (args, "x")) == 2);
  ++f->tools; *out = mulib_json_string ("3"); return *out ? 1 : -3;
}
typedef struct tls_fixture { size_t offset; unsigned closes, events; int bad_handshake, bad_eof; } tls_fixture;
static int tls_handshake (void *u, int *verified)
{ tls_fixture *f = (tls_fixture *)u; *verified = !f->bad_handshake; return 0; }
static int tls_write (void *u, const void *p, size_t n, size_t *written)
{ (void)u; (void)p; *written = n > 3 ? 3 : n; return 0; }
static int tls_read (void *u, void *p, size_t cap, size_t *read)
{
  const char wire[] = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\n\r\ndata: {\"type\":\"test\"}\n\ndata: [DONE]\n\n";
  tls_fixture *f = (tls_fixture *)u;
  size_t n = sizeof wire - 1 - f->offset;
  if (!n) { *read = 0; return f->bad_eof ? MULIB_TSL_EBACKEND : MULIB_TSL_EOF; }
  if (n > 7) n = 7;
  if (n > cap) n = cap;
  memcpy (p, wire + f->offset, n); f->offset += n; *read = n; return 0;
}
static void tls_close (void *u)
{ ++((tls_fixture *)u)->closes; }
static int tls_connect (void *u, const mulib_uri_t *uri, mulib_tsl_t *out)
{
  mulib_tsl_ops_t ops = {tls_handshake, tls_read, tls_write, tls_close};
  assert (uri->scheme.length == 5 && uri->port.length == 3 && !memcmp (uri->port.data, "443", 3));
  return mulib_tsl_init (out, &ops, u);
}
static int tls_event (void *u, const mulib_openai_event_t *e)
{
  tls_fixture *f = (tls_fixture *)u;
  assert (!f->closes); ++f->events;
  assert (f->events == 1 ? e->json != NULL : e->done && !e->json); return 1;
}
static void https_stream (void)
{
  for (int mode = 0; mode < 3; ++mode) {
    tls_fixture f = {0};
    mulib_openai_client_t c = mulib_openai_client_default ("synthetic-key");
    mulib_openai_request_t q = {0}; mulib_openai_response_t r = {0};
    f.bad_handshake = mode == 1; f.bad_eof = mode == 2;
    c.base_url = "https://fixture/v1"; c.transport.tls = tls_connect; c.transport.tls_user = &f;
    q.operation_id = "createResponse"; q.body = "{\"stream\":true}"; q.body_length = strlen ((const char *)q.body);
    q.content_type = "application/json"; q.event = tls_event; q.event_user = &f;
    assert (mulib_openai_client_request (&c, &q, &r) == (mode ? -4 : 1));
    assert (f.closes == 1 && f.events == (mode == 1 ? 0u : 2u));
    if (mode) assert (!r.http.url); else assert (r.stream_done);
    mulib_openai_response_free (&r);
  }
}
static void catalog (void)
{
  assert (mulib_openai_operation_count () == 388);
  for (size_t i = 0; i < mulib_openai_operation_count (); ++i) {
    const mulib_openai_operation_t *o = mulib_openai_operation_at (i);
    mulib_http_header_t params[12]; char names[12][100]; size_t count = 0;
    const char *p = o->path; char *target = NULL;
    assert (mulib_openai_operation (o->id) == o);
    while ((p = strchr (p, '{')) != NULL) {
      const char *end = strchr (++p, '}'); size_t n = (size_t)(end - p);
      assert (count < 12 && n < 100); memcpy (names[count], p, n); names[count][n] = 0;
      params[count].name = names[count]; params[count].value = (char *)"id/slash ?#"; ++count; p = end + 1;
    }
    assert (mulib_openai_target (o, params, count, NULL, 0, 0, &target) == 1);
    assert (mulib_openai_match (o->method, target) == o);
    for (size_t j = 0; j < count; ++j) {
      char *value = NULL;
      assert (mulib_openai_path_parameter (o, target, names[j], &value) == 1);
      assert (!strcmp (value, params[j].value)); free (value);
    }
    free (target);
  }
  {
    char *s = NULL;
    mulib_http_header_t p = {(char *)"response_id", (char *)".."};
    assert (mulib_openai_target (mulib_openai_operation ("getResponse"), &p, 1, NULL, 0, 0, &s) == 1);
    assert (!strcmp (s, "/responses/%2E%2E")); free (s);
    assert (mulib_openai_query ("/x?a=a%2Bb+%26", "a", &s) == 1 && !strcmp (s, "a+b &")); free (s);
    assert (mulib_openai_query ("/x?a=%00", "a", &s) == -1);
    assert (mulib_openai_query ("/x?a=%ZZ", "a", &s) == -1);
    assert (!strcmp (mulib_openai_match ("GET", "/responses/id?other=1&beta=true")->id, "beta_getResponse"));
    assert (!strcmp (mulib_openai_match ("POST", "/responses/input_tokens")->id, "Getinputtokencounts"));
  }
}
int main (void)
{
  fixture f = {0};
  mulib_openai_client_t c = mulib_openai_client_default ("synthetic-key");
  mulib_openai_request_t q = {0};
  mulib_openai_response_t r = {0};
  mulib_json_t *body = mulib_openai_responses_request ("synthetic-model", "hi \"world\"");
  catalog (); https_stream (); assert (body);
  f.server = mulib_openai_server_default (handler, &f); f.server.api_key = "synthetic-key";
  c.base_url = "http://fixture/v1/"; c.transport.http = transport; c.transport.http_user = &f;
  c.project = "synthetic-project";
  assert (mulib_openai_client_json (&c, "createResponse", body, &r) == 1);
  { mulib_json_t *text = mulib_openai_output_text (r.json); assert (text);
    assert (!strcmp (mulib_json_text (text, NULL), "hello world")); mulib_json_destroy (text); }
  mulib_openai_response_free (&r);
  q.operation_id = "createResponse"; q.json = body; q.event = event; q.event_user = &f;
  f.mode = 1;
  assert (mulib_json_set (body, "stream", mulib_json_bool (1)));
  assert (mulib_openai_client_request (&c, &q, &r) == 1 && r.stream_done && f.events == 2);
  assert (r.http.message.body_length == 0); mulib_openai_response_free (&r);
  f.events = 0; f.cancel = 1;
  assert (mulib_openai_client_request (&c, &q, &r) == -7); assert (!r.http.url);
  f.cancel = 0; q.event = NULL;
  assert (mulib_json_set (body, "stream", mulib_json_bool (0)));
  f.mode = 2;
  assert (mulib_openai_client_request (&c, &q, &r) == -8 && r.http.message.status == 429 && r.json);
  assert (mulib_json_get (r.json, "error")); mulib_openai_response_free (&r);
  q.event = event;
  assert (mulib_openai_client_request (&c, &q, &r) == -8 && r.json); mulib_openai_response_free (&r);
  q.event = NULL; f.mode = 3;
  assert (mulib_openai_client_run (&c, body, 3, tool, &f, &r) == 1 && f.tools == 1);
  mulib_openai_response_free (&r);
  assert (mulib_openai_client_run (&c, body, 1, tool, &f, &r) == -2 && f.tools == 1);
  mulib_openai_response_free (&r);
  memset (&q, 0, sizeof q); q.operation_id = "listFiles";
  assert (mulib_openai_client_pages (&c, &q, 5, item, &f, &r) == 1 && f.items == 2);
  mulib_openai_response_free (&r);
  assert (mulib_openai_client_pages (&c, &q, 1, item, &f, &r) == -2); mulib_openai_response_free (&r);
  {
    mulib_multipart_part_t parts[2] = {
      {(char *)"purpose", NULL, NULL, "assistants", 10},
      {(char *)"file", (char *)"sample.bin", (char *)"application/octet-stream", "a\0bc", 4}
    };
    q.operation_id = "createFile";
    assert (mulib_openai_client_multipart (&c, &q, "MuLIB-boundary", parts, 2, &r) == 1);
    mulib_openai_response_free (&r);
    parts[1].filename = (char *)"bad\r\nfile";
    assert (mulib_openai_client_multipart (&c, &q, "MuLIB-boundary", parts, 2, &r) == -1);
  }
  {
    mulib_http_header_t path = {(char *)"file_id", (char *)"x/y ?#"};
    q.operation_id = "downloadFile"; q.path = &path; q.path_count = 1;
    assert (mulib_openai_client_request (&c, &q, &r) == 1);
    assert (r.http.message.body_length == 4 && !memcmp (r.http.message.body, "a\0bc", 4));
    mulib_openai_response_free (&r); q.path = NULL; q.path_count = 0;
  }
  q.operation_id = "listModels";
  assert (mulib_openai_client_request (&c, &q, &r) == -8 && r.http.message.status == 501); mulib_openai_response_free (&r);
  c.api_key = "wrong";
  assert (mulib_openai_client_request (&c, &q, &r) == -8 && r.http.message.status == 401); mulib_openai_response_free (&r);
  c.api_key = "bad\r\nX: injected";
  assert (mulib_openai_client_request (&c, &q, &r) == -1);
  c.api_key = "synthetic-key";
  q.operation_id = NULL; q.method = "PUT"; q.target = "/models";
  assert (mulib_openai_client_request (&c, &q, &r) == -8 && r.http.message.status == 405); mulib_openai_response_free (&r);
  q.target = "/unknown";
  assert (mulib_openai_client_request (&c, &q, &r) == -8 && r.http.message.status == 404); mulib_openai_response_free (&r);
  q.method = "POST"; q.target = "/responses"; q.content_type = "application/json"; q.body = "{"; q.body_length = 1;
  assert (mulib_openai_client_request (&c, &q, &r) == -8 && r.http.message.status == 400); mulib_openai_response_free (&r);
  q.content_type = "text/plain";
  assert (mulib_openai_client_request (&c, &q, &r) == -8 && r.http.message.status == 415); mulib_openai_response_free (&r);
  q.target = "//other.example/";
  assert (mulib_openai_client_request (&c, &q, &r) == -1);
  mulib_json_destroy (body);
  puts ("OpenAI catalog, client/server, streaming, tools, pagination and multipart passed");
  return 0;
}
