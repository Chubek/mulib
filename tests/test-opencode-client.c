#include "fixtures/opencode-client-setup.h"
#include <assert.h>
#include <stdio.h>

typedef struct fixture {
  const char *method, *target, *accept, *auth, *body, *wire;
  size_t calls;
} fixture;

static int mock_http(void *user, const mulib_uri_t *uri, const void *p, size_t n,
                     mulib_webclient_receive_fn receive, void *receiver)
{
  fixture *f = (fixture *)user;
  mulib_http_message_t req = {0};
  size_t used, length = strlen(f->wire);
  assert(uri->host.length == 9 && !memcmp(uri->host.data, "unit.test", 9));
  assert(mulib_http_parse(p, n, 0, NULL, &req, &used) == 1 && used == n);
  assert(!strcmp(req.target, f->target) && !strcmp(req.method, f->method));
  assert(!strcmp(mulib_http_header(&req, "Accept"), f->accept));
  if (f->auth) assert(!strcmp(mulib_http_header(&req, "Authorization"), f->auth));
  else assert(!mulib_http_header(&req, "Authorization"));
  if (f->body) {
    assert(req.body_length == strlen(f->body) && !memcmp(req.body, f->body, req.body_length));
    assert(!strcmp(mulib_http_header(&req, "Content-Type"), "application/json"));
  } else assert(!req.body_length);
  mulib_http_message_free(&req);
  ++f->calls;
  for (size_t at = 0; at < length;) {
    size_t chunk = length - at;
    if (chunk > 3) chunk = 3;
    if (!receive(receiver, f->wire + at, chunk)) return 0;
    at += chunk;
  }
  return 1;
}
static int got_event(void *user, const mulib_sse_event_t *event, const mulib_json_t *doc)
{
  unsigned *seen = (unsigned *)user;
  const char *type = mulib_json_text(mulib_json_get(doc,"type"), NULL);
  assert(type && !strcmp(type, "server.connected"));
  assert(!strcmp(event->id, "42") && event->data_length);
  return ++*seen == 1;
}
static const char json_ok[] = "HTTP/1.1 200 OK\r\nContent-Type: Application/JSON; charset=utf-8\r\n"
  "Content-Length: 15\r\n\r\n{\"version\":\"2\"}";
static const char forbidden[] = "HTTP/1.1 403 Forbidden\r\nContent-Type: application/json\r\n"
  "Content-Length: 16\r\n\r\n{\"error\":\"deny\"}";
static const char event_ok[] = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\n\r\n"
  ": first\r\nid: 42\r\ndata: {\"type\":\r\ndata: \"server.connected\"}\r\n\r\n";
static const char event_failure[] = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\n\r\n"
  "event: effect/http-api/stream/failure\ndata: []\n\n";

static int loopback(const char *origin)
{
  mulib_cdd_opencode_client_t c = mulib_cdd_opencode_client(origin);
  mulib_cdd_opencode_request_t q = {0};
  mulib_cdd_opencode_response_t r = {0};
  const mulib_cdd_opencode_param_t loc = {"location[directory]", "/workspace a&b"};
  unsigned events = 0;
  c.allow_http = 1;
  c.authorization = "Bearer fixture";
  c.transport.options.timeout_ms = 3000;
  q.operation_id = "agent.list"; q.query = &loc; q.query_count = 1;
  assert(mulib_cdd_opencode_request(&c,&q,&r) == MULIB_CDD_OPENCODE_OK);
  assert(mulib_json_typeof(r.json) == MULIB_JSON_ARRAY && mulib_json_size(r.json) == 1);
  mulib_cdd_opencode_response_free(&r);
  q.operation_id = "event.subscribe"; q.query = NULL; q.query_count = 0;
  q.event = got_event; q.event_user = &events;
  assert(mulib_cdd_opencode_request(&c,&q,&r) == MULIB_CDD_OPENCODE_OK && events == 1);
  assert(r.http.message.status == 200 && !r.json && !r.http.message.body_length);
  mulib_cdd_opencode_response_free(&r);
  return 0;
}

int main(int argc, char **argv)
{
  if (argc == 2) return loopback(argv[1]);
  assert(argc == 1);
  mulib_cdd_opencode_client_t c = mulib_cdd_opencode_client("http://unit.test:4096/");
  mulib_cdd_opencode_request_t q = {0};
  mulib_cdd_opencode_response_t r = {0};
  unsigned char empty[sizeof r];
  fixture f = {"GET", "/api/info", "application/json", "Bearer fixture", NULL, json_ok, 0};
  const mulib_cdd_opencode_param_t params[] = {
    {"types", "chat"}, {"location[directory]", "/repo/a b?#"}, {"types", "image"},
    {"sessionID", "ses%2f.."}, {"sessionID", "ses_4"}, {"messageID", "msg_1"}
  };
  const mulib_cdd_opencode_param_t mode = {"mode", "working"};
  size_t calls;
  memcpy(empty, &r, sizeof r);
  for (size_t i = 0; i < mulib_cdd_opencode_operation_count(); ++i) {
    const mulib_cdd_opencode_operation_t *op = mulib_cdd_opencode_operation_at(i);
    assert(op && op == mulib_cdd_opencode_operation(op->id));
    if (i) assert(strcmp(mulib_cdd_opencode_operation_at(i-1)->id, op->id) < 0);
  }
  assert(!mulib_cdd_opencode_operation_at(mulib_cdd_opencode_operation_count()));
  assert(!mulib_cdd_opencode_operation("shell.create"));
  c.transport.http = mock_http; c.transport.http_user = &f;
  c.authorization = f.auth;
  q.operation_id = "server.info";
  assert(mulib_cdd_opencode_request(&c, &q, &r) == MULIB_CDD_OPENCODE_INVALID && !f.calls);
  c.allow_http = 1;
  c.authorization = "a\r\nX-Dup: b";
  assert(mulib_cdd_opencode_request(&c, &q, &r) == MULIB_CDD_OPENCODE_INVALID && !f.calls);
  c.authorization = f.auth;
  c.origin = "http://unit.test:4096/api?origin=yes";
  assert(mulib_cdd_opencode_request(&c, &q, &r) == MULIB_CDD_OPENCODE_INVALID && !f.calls);
  c.origin = "http://unit.test:4096/";
  assert(mulib_cdd_opencode_request(&c, &q, &r) == 1 && f.calls == 1);
  assert(!strcmp(mulib_json_text(mulib_json_get(r.json, "version"), NULL), "2"));
  mulib_cdd_opencode_response_free(&r);
  q.operation_id = "model.list";
  q.query = params; q.query_count = 3;
  f.target = "/api/model?types=chat&location%5Bdirectory%5D=%2Frepo%2Fa%20b%3F%23&types=image";
  assert(mulib_cdd_opencode_request(&c, &q, &r) == 1 && f.calls == 2);
  mulib_cdd_opencode_response_free(&r);
  q.query = NULL; q.query_count = 0;
  q.operation_id = "vcs.diff";
  assert(mulib_cdd_opencode_request(&c, &q, &r) == -1 && f.calls == 2);
  q.query = params; q.query_count = 1;
  assert(mulib_cdd_opencode_request(&c, &q, &r) == -1 && f.calls == 2);
  q.query = &mode; q.query_count = 1; f.target = "/api/vcs/diff?mode=working";
  assert(mulib_cdd_opencode_request(&c,&q,&r) == 1);
  mulib_cdd_opencode_response_free(&r);
  q.query = NULL; q.query_count = 0;
  q.operation_id = "session.message.get"; q.path = params + 3; q.path_count = 2;
  assert(mulib_cdd_opencode_request(&c,&q,&r) == -1 && f.calls == 3);
  q.path++; f.target = "/api/session/ses_4/message/msg_1";
  assert(mulib_cdd_opencode_request(&c,&q,&r) == 1);
  mulib_cdd_opencode_response_free(&r);
  q.operation_id = "session.update"; q.path_count = 1;
  q.json = mulib_json_parse("{\"title\":\"sample\"}", 18, NULL);
  assert(q.json);
  assert(mulib_cdd_opencode_request(&c,&q,&r) == MULIB_CDD_OPENCODE_UNSUPPORTED);
  c.allow_session_writes = 1;
  calls = f.calls;
  {
    const mulib_json_t *saved = q.json;
    q.json = mulib_json_parse("{\"permissions\":[]}", 18, NULL);
    assert(q.json && mulib_cdd_opencode_request(&c,&q,&r) == -1 && f.calls == calls);
    mulib_json_destroy((mulib_json_t *)q.json);
    q.json = saved;
  }
  f.method = "PATCH"; f.target = "/api/session/ses_4"; f.body = "{\"title\":\"sample\"}";
  f.wire = "HTTP/1.1 204 No Content\r\n\r\n";
  assert(mulib_cdd_opencode_request(&c,&q,&r) == 1 && r.http.message.status == 204 && !r.json);
  mulib_cdd_opencode_response_free(&r); mulib_json_destroy((mulib_json_t *)q.json);
  q.json = NULL; q.path = NULL; q.path_count = 0;
  q.operation_id = "event.subscribe";
  f.target = "/api/event"; f.method = "GET"; f.accept = "text/event-stream"; f.body = NULL;
  unsigned seen = 0;
  q.event = got_event; q.event_user = &seen;
  f.wire = event_ok;
  assert(mulib_cdd_opencode_request(&c,&q,&r) == 1 && seen == 1 && !r.json);
  mulib_cdd_opencode_response_free(&r);
  assert(mulib_cdd_opencode_request(&c,&q,&r) == MULIB_CDD_OPENCODE_CANCELLED && seen == 2);
  assert(!memcmp(empty, &r, sizeof r));
  f.wire = event_failure;
  assert(mulib_cdd_opencode_request(&c,&q,&r) == MULIB_CDD_OPENCODE_STREAM_ERROR);
  f.wire = forbidden;
  assert(mulib_cdd_opencode_request(&c,&q,&r) == MULIB_CDD_OPENCODE_HTTP_ERROR);
  assert(r.http.message.status == 403 && r.json && r.http.message.body_length == 16);
  mulib_cdd_opencode_response_free(&r);
  q.event = NULL; q.operation_id = "server.info";
  f.target = "/api/info"; f.accept = "application/json";
  f.wire = "HTTP/1.1 302 Found\r\nLocation: https://other.test/\r\nContent-Length: 0\r\n\r\n";
  c.transport.options.max_redirects = 8;
  calls = f.calls;
  assert(mulib_cdd_opencode_request(&c,&q,&r) == MULIB_CDD_OPENCODE_HTTP_ERROR);
  assert(f.calls == calls+1 && r.http.message.status == 302 && !r.http.redirects);
  mulib_cdd_opencode_response_free(&r);
  f.wire = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Type: text/html\r\n"
    "Content-Length: 2\r\n\r\n{}";
  assert(mulib_cdd_opencode_request(&c,&q,&r) == MULIB_CDD_OPENCODE_PROTOCOL);
  f.wire = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 2\r\n\r\n{x";
  assert(mulib_cdd_opencode_request(&c,&q,&r) == MULIB_CDD_OPENCODE_PROTOCOL);
  c.transport.options.web.http.max_body_bytes = 2;
  f.wire = json_ok;
  assert(mulib_cdd_opencode_request(&c,&q,&r) == MULIB_CDD_OPENCODE_LIMIT);
  c.transport.options.web.http.max_body_bytes = 0;

  mulib_mune_state_t *s = mulib_mune_state_create(NULL);
  mulib_mune_result_t *result = NULL;
  static const char script[] =
    "local b,h,s=opencode_request('server.info'); assert(h==200 and s==1)\n"
    "local a,c,d=opencode_request('session.prompt'); assert(a==nil and c==nil and d==-10)\n"
    "local x,y,z=opencode_request('http://other.test'); assert(x==nil and y==nil and z==-10)\n"
    "return b\n";
  mulib_mune_error_t error;
  assert(s && mulib_cdd_opencode_bind(s,&c) == MULIB_MUNE_OK);
  calls = f.calls;
  if (mulib_mune_run_source(s,script,sizeof script-1,&result,&error) != MULIB_MUNE_OK) {
    fprintf(stderr,"Mune offset %zu: %s\n",error.offset,error.message);
    return 1;
  }
  assert(f.calls == calls+1 && mulib_mune_result_count(result) == 1);
  assert(!strcmp(mulib_mune_value_string(mulib_mune_result_get(result,0),NULL),"{\"version\":\"2\"}"));
  mulib_mune_result_destroy(result);
  mulib_mune_state_destroy(s);
  puts("OpenCode V2 client: routes, JSON, streaming, transport policy and Mune binding OK");
  return 0;
}
