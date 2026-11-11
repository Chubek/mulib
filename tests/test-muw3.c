#include "fixtures/muw3-setup.h"
#include <assert.h>
#include <stdio.h>
typedef struct mock { unsigned calls; int mode; } mock;
static int transport (void *user, const mulib_uri_t *endpoint, const void *wire, size_t n,
    mulib_webclient_receive_fn receive, void *receiver)
{
  mock *m = (mock *)user;
  mulib_http_message_t request = {0};
  size_t used;
  const char *response;
  (void)endpoint;
  assert (mulib_http_parse (wire, n, 0, NULL, &request, &used) == MULIB_HTTP_OK && used == n);
  ++m->calls;
  if (m->mode == 1) response = "HTTP/1.1 302 Found\r\nLocation: /loop\r\nContent-Length: 0\r\n\r\n";
  else if (m->mode == 2) {
    assert (request.chunked && request.body_length == 3 && !memcmp (request.body, "a\0b", 3));
    response = m->calls == 1 ? "HTTP/1.1 307 Temporary Redirect\r\nLocation: ../next\r\nContent-Length: 0\r\n\r\n"
      : "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
  } else if (m->mode == 3) response = "HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\na";
  else if (m->mode == 4) response = "HTTP/1.1 302 Found\r\nLocation: https://other.test/\r\nContent-Length: 0\r\n\r\n";
  else if (m->calls == 1) {
    assert (!strcmp (request.method, "POST") && !strcmp (request.target, "/start"));
    assert (mulib_http_header (&request, "Authorization") && mulib_http_header (&request, "Cookie"));
    response = "HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 302 Found\r\nLocation: http://other.test/next#part\r\nContent-Length: 0\r\n\r\n";
  } else {
    assert (!strcmp (request.method, "GET") && !strcmp (request.target, "/next"));
    assert (!request.body_length && !mulib_http_header (&request, "Authorization")
        && !mulib_http_header (&request, "Cookie") && !mulib_http_header (&request, "Content-Type"));
    static const char reply[] = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nSet-Cookie: a=1\r\nSet-Cookie: b=2\r\n\r\n"
      "3;name=\"a\\\"b\"\r\na\0b\r\n0;end\r\nDigest: abc\r\n\r\n";
    mulib_http_message_free (&request);
    return receive (receiver, reply, sizeof reply - 1);
  }
  mulib_http_message_free (&request);
  for (size_t i = 0; i < strlen (response); ++i) if (!receive (receiver, response + i, 1)) return 0;
  return 1;
}
static void metadata (void)
{
  static const char wire[] = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n2;x=y;flag\r\nhi\r\n0\r\nDigest: abc\r\n\r\n";
  static const char bad[] = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n0\r\nContent-Length: 0\r\n\r\n";
  mulib_http_options_t o = {0,0,0,MULIB_HTTP_CHUNK_METADATA};
  mulib_http_message_t m = {0}, sentinel = {0}, round = {0};
  unsigned char *encoded = NULL;
  size_t used = 99, length;
  sentinel.status = 599;
  for (size_t i = 0; i < sizeof wire - 1; ++i) {
    m = sentinel;
    assert (mulib_http_parse (wire, i, 1, &o, &m, &used) == MULIB_HTTP_MORE);
    assert (!used && m.status == 599 && !m.trailers);
  }
  assert (mulib_http_parse (wire, sizeof wire - 1, 1, &o, &m, &used) == 1);
  assert (m.trailer_count == 1 && !strcmp (m.trailers[0].value, "abc") && !mulib_http_header (&m, "Digest"));
  assert (mulib_http_serialize (&m, &o, &encoded, &length) == 1);
  assert (mulib_http_parse (encoded, length, 1, &o, &round, &used) == 1 && round.trailer_count == 1);
  mulib_http_message_free (&m); mulib_http_message_free (&round); free (encoded);
  assert (mulib_http_parse (wire, sizeof wire - 1, 1, NULL, &m, &used) == -1);
  assert (mulib_http_parse (bad, sizeof bad - 1, 1, &o, &m, &used) == -1);
  {
    static const char nominated[] = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nConnection: X-Hop\r\n\r\n0\r\nX-Hop: bad\r\n\r\n";
    static const char *const malformed[] = {"2;\r\nhi\r\n0\r\n\r\n", "2;x=\"unterminated\r\nhi\r\n0\r\n\r\n",
      "2;x=\r\nhi\r\n0\r\n\r\n", "2;x=\"\177\"\r\nhi\r\n0\r\n\r\n", "2; x = y garbage\r\nhi\r\n0\r\n\r\n"};
    assert (mulib_http_parse (nominated, sizeof nominated - 1, 1, &o, &m, &used) == -1);
    for (size_t i = 0; i < sizeof malformed / sizeof *malformed; ++i) {
      char text[256];
      int n = snprintf (text, sizeof text, "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n%s", malformed[i]);
      assert (n > 0 && n < (int)sizeof text);
      assert (mulib_http_parse (text, (size_t)n, 1, &o, &m, &used) == -1);
    }
  }
  o.max_headers = 1;
  assert (mulib_http_parse (wire, sizeof wire - 1, 1, &o, &m, &used) == -2);
}
int main (void)
{
  mulib_muw3_client_t c = mulib_muw3_client_default ();
  mulib_http_header_t h[] = {{"Authorization","Bearer secret"},{"Cookie","a=1"},{"Content-Type","text/plain"}};
  mulib_muw3_request_t q = {"http://first.test/start#ignored", "POST", h, 3, "a\0b", 3};
  mulib_muw3_response_t r = {0};
  mock m = {0};
  size_t cursor = 0;
  c.http = transport; c.http_user = &m;
  assert (mulib_muw3_perform (&c, &q, &r) == 1 && m.calls == 2 && r.redirects == 1);
  assert (!strcmp (r.url, "http://other.test/next") && r.message.body_length == 3
      && !memcmp (r.message.body, "a\0b", 3) && r.message.trailer_count == 1);
  assert (!strcmp (mulib_http_header_next (&r.message, "Set-Cookie", &cursor), "a=1"));
  assert (!strcmp (mulib_http_header_next (&r.message, "set-cookie", &cursor), "b=2"));
  assert (!mulib_http_header_next (&r.message, "Set-Cookie", &cursor));
  mulib_muw3_response_free (&r);
  m.calls = 0; m.mode = 1; c.options.max_redirects = 2;
  r.redirects = 99;
  assert (mulib_muw3_perform (&c, &q, &r) == MULIB_MUW3_REDIRECT && m.calls == 3 && r.redirects == 99 && !r.url);
  c.options.max_redirects = 0;
  assert (mulib_muw3_perform (&c, &q, &r) == 1 && r.message.status == 302);
  mulib_muw3_response_free (&r);
  m.calls = 0; m.mode = 2; c.options.max_redirects = 2;
  h[0].name = "Transfer-Encoding"; h[0].value = "chunked"; q.header_count = 1;
  assert (mulib_muw3_perform (&c, &q, &r) == 1 && m.calls == 2);
  mulib_muw3_response_free (&r);
  m.mode = 3;
  assert (mulib_muw3_perform (&c, &q, &r) == MULIB_MUW3_INVALID);
  m.mode = 4;
  c.options.ca_file = "/dev/null";
  assert (mulib_muw3_perform (&c, &q, &r) == MULIB_MUW3_TRANSPORT);
  q.url = "http://u:p@example.test/";
  assert (mulib_muw3_perform (&c, &q, &r) == MULIB_MUW3_INVALID);
  {
    mulib_http_header_t fields[] = {{"a b","+&/"},{"utf8","\xc3\xa9"}};
    char *encoded = NULL;
    assert (mulib_muw3_form (fields, 2, 0, &encoded) == 1 && !strcmp (encoded, "a+b=%2B%26%2F&utf8=%C3%A9"));
    free (encoded); encoded = NULL;
    assert (mulib_muw3_form (fields, 2, 4, &encoded) == -2 && !encoded);
  }
  metadata ();
  puts ("MuW3: redirects, framing, metadata, limits and binary requests passed");
  return 0;
}
