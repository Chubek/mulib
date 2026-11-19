#include "mulib-sse-client.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

struct fixture {
  const char *wire, *id;
  size_t length, stride, requests;
};

static int transport(void *user, const mulib_uri_t *uri, const void *request,
    size_t length, mulib_webclient_receive_fn receive, void *receiver) {
  struct fixture *f = (struct fixture *)user;
  mulib_http_message_t parsed = {0};
  const char *id;
  size_t consumed;
  (void)uri;
  ++f->requests;
  assert(mulib_http_parse(request, length, 0, NULL, &parsed, &consumed) == MULIB_HTTP_OK);
  assert(consumed == length && !strcmp(parsed.method, "GET") && !strcmp(parsed.target, "/events"));
  assert(!strcmp(mulib_http_header(&parsed, "Accept"), "text/event-stream"));
  assert(!strcmp(mulib_http_header(&parsed, "Cache-Control"), "no-cache"));
  assert(!strcmp(mulib_http_header(&parsed, "Accept-Encoding"), "identity"));
  id = mulib_http_header(&parsed, "Last-Event-ID");
  assert(f->id ? id && !strcmp(id, f->id) : id == NULL);
  mulib_http_message_free(&parsed);
  for (size_t off = 0; off < f->length;) {
    size_t n = f->length - off;
    if (n > f->stride) n = f->stride;
    if (!receive(receiver, f->wire + off, n)) return 0;
    off += n;
  }
  return 1;
}

struct events {
  size_t count;
  int stop, empty;
};

static int on_event(void *user, const mulib_sse_event_t *event) {
  struct events *e = (struct events *)user;
  if (e->empty) {
    assert(!strcmp(event->event, "message") && !*event->id && !event->data_length);
  } else if (e->count == 0) {
    static const char data[] = "hello \xe2\x82\xac \xf0\x9f\x8c\x8d\n"
        "\0bye \xef\xbf\xbd!"
        "\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd";
    assert(!strcmp(event->event, "notice") && !strcmp(event->id, "123"));
    assert(event->data_length == sizeof data - 1 && !memcmp(event->data, data, sizeof data - 1));
  } else {
    assert(e->count == 1 && !strcmp(event->event, "message") && !strcmp(event->id, "123"));
    assert(event->data_length == 0);
  }
  ++e->count;
  return e->stop ? e->stop - 1 : 1;
}

static void streaming(void) {
  static const char body[] = "\xef\xbb\xbf: comment\r\nid: 123\r\n"
      "event: notice\rdata: hello \xe2\x82\xac \xf0\x9f\x8c\x8d\r\n"
      "data: \0bye \xe2!\xed\xa0\x80\n\rretry: 100\nid: ignored\0id\ndata:\n\n"
      "data: lost\r\nid: discarded\xe2";
  static const char headers[] = "HTTP/1.1 103 Early Hints\r\nLink: </>\r\n\r\n"
      "HTTP/1.1 200 OK\r\nContent-Type: TeXt/EVent-STReaM; charset=\"utf-8\"; x=\"\\\";,\"\r\n"
      "Content-Encoding: identity\r\nTransfer-Encoding: chunked\r\n\r\n";
  char wire[4096];
  size_t pos = sizeof headers - 1;
  struct fixture f = {wire, "r\xc3\xa9sume", 0, 1, 0};
  mulib_muw3_client_t c = mulib_muw3_client_default();
  mulib_cdd_sse_client_options_t opt = {f.id, 256};
  mulib_muw3_response_t out = {0};
  memcpy(wire, headers, pos);
  /* Each SSE byte is in a separate HTTP chunk, independently of transport cuts. */
  for (size_t i = 0; i < sizeof body - 1; ++i) {
    memcpy(wire + pos, "1\r\nX\r\n", 6);
    wire[pos + 3] = body[i];
    pos += 6;
  }
  memcpy(wire + pos, "0\r\nX-End: yes\r\n\r\n", sizeof "0\r\nX-End: yes\r\n\r\n" - 1);
  f.length = pos + sizeof "0\r\nX-End: yes\r\n\r\n" - 1;
  c.http = transport;
  c.http_user = &f;
  for (; f.stride <= 31; ++f.stride) {
    struct events e = {0};
    assert(mulib_cdd_sse_client_get(&c, "http://example.test/events", &opt, on_event, &e, &out) == 1);
    assert(e.count == 2 && out.message.status == 200 && out.message.body_length == 0);
    assert(!out.message.body && out.message.trailer_count == 1 && out.redirects == 0);
    mulib_muw3_response_free(&out);
  }
  {
    struct events e = {.stop = 1};
    out.message.status = 418;
    assert(mulib_cdd_sse_client_get(&c, "http://example.test/events", &opt, on_event, &e, &out)
        == MULIB_CDD_SSE_CLIENT_STOP);
    assert(e.count == 1 && out.message.status == 418 && !out.url);
    e.count = 0;
    e.stop = 3;
    assert(mulib_cdd_sse_client_get(&c, "http://example.test/events", &opt, on_event, &e, &out)
        == MULIB_CDD_SSE_CLIENT_STOP);
    assert(e.count == 1 && out.message.status == 418 && !out.url);
    e.count = 0;
    assert(mulib_sse_client_get(&c, "http://example.test/events", &opt, on_event, &e, &out)
        == MULIB_CDD_SSE_CLIENT_EVENT);
    assert(e.count == 1 && out.message.status == 418 && !out.url);
  }
  opt.max_event_bytes = 8;
  {
    struct events e = {0};
    assert(mulib_cdd_sse_client_get(&c, "http://example.test/events", &opt, on_event, &e, &out) == -2);
    assert(mulib_sse_client_get(&c, "http://example.test/events", &opt, on_event, &e, &out) == -9);
    assert(e.count == 0 && out.message.status == 418 && !out.url);
  }
}

static void metadata(void) {
  static const struct {
    const char *wire;
    int result;
    unsigned status;
    size_t events;
  } cases[] = {
    {"HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n", -8, 0, 0},
    {"HTTP/1.1 200 OK\r\nContent-Type: text/event-stream bad\r\nContent-Length: 0\r\n\r\n", -8, 0, 0},
    {"HTTP/1.1 200 OK\r\nContent-Type: text/event-stream-what\r\nContent-Length: 0\r\n\r\n", -8, 0, 0},
    {"HTTP/1.1 200 OK\r\nContent-Type: text/event-stream; x=\"unterminated\r\nContent-Length: 0\r\n\r\n", -8, 0, 0},
    {"HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\n"
     "content-type: text/event-stream\r\nContent-Length: 0\r\n\r\n", -8, 0, 0},
    {"HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\n"
     "Content-Encoding: gzip\r\nContent-Length: 0\r\n\r\n", -8, 0, 0},
    {"HTTP/1.1 200 OK\r\nContent-Type: text/event-stream; charset=latin1\r\n"
     "Content-Length: 0\r\n\r\n", 1, 200, 0},
    {"HTTP/1.1 204 No Content\r\n\r\n", 1, 204, 0},
    {"HTTP/1.1 302 Found\r\nLocation: http://example.test/else\r\n"
     "Content-Length: 7\r\n\r\ndata:\n\n", 1, 302, 0},
    {"HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\n"
     "Content-Length: 8\r\n\r\ndata:\n\n", -1, 0, 1},
    {"HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\n\r\ndata:\n\n", 1, 200, 1}
  };
  mulib_muw3_client_t c = mulib_muw3_client_default();
  struct fixture f = {NULL, NULL, 0, 123, 0};
  mulib_muw3_response_t out = {.message.status = 418};
  mulib_cdd_sse_client_options_t opt = {"", 0};
  c.http = transport;
  c.http_user = &f;
  for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i) {
    struct events e = {.empty = 1};
    int rc;
    f.wire = cases[i].wire;
    f.length = strlen(f.wire);
    rc = mulib_cdd_sse_client_get(&c, "http://example.test/events", &opt, on_event, &e, &out);
    assert(rc == cases[i].result && f.requests == i + 1 && e.count == cases[i].events);
    if (rc == 1) {
      assert(out.message.status == cases[i].status && !out.redirects);
      mulib_muw3_response_free(&out);
      out.message.status = 418;
    } else assert(out.message.status == 418 && !out.url);
  }
  {
    struct events e = {0};
    char id[8194];
    size_t requests = f.requests;
    opt.last_event_id = "invalid\r\nfield";
    assert(mulib_cdd_sse_client_get(&c, "http://example.test/events", &opt, on_event, &e, &out) == -1);
    opt.last_event_id = "\xff";
    assert(mulib_cdd_sse_client_get(&c, "http://example.test/events", &opt, on_event, &e, &out) == -1);
    memset(id, 'x', sizeof id - 1);
    id[sizeof id - 1] = '\0';
    opt.last_event_id = id;
    assert(mulib_cdd_sse_client_get(&c, "http://example.test/events", &opt, on_event, &e, &out) == -2);
    assert(!e.count && requests == f.requests && out.message.status == 418);
  }
}

int main(void) {
  streaming();
  metadata();
  return 0;
}
