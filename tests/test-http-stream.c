#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_URI
#define MULIB_IMPL_HTTP
#include "mulib-http.h"
#include <assert.h>
#include <string.h>
typedef struct collected { mulib_buffer_t body; unsigned headers; int cancel; } collected;
static int collect (void *user, const mulib_http_message_t *m, const void *p, size_t n)
{
  collected *c = (collected *)user;
  assert (m->status >= 200);
  if (!p) ++c->headers;
  return !c->cancel && mulib_buffer_append (&c->body, p, n);
}
static void valid (const char *wire, size_t n, const char *body, size_t length, int trailers)
{
  mulib_http_options_t o = {0, 0, 0, MULIB_HTTP_CHUNK_METADATA};
  for (size_t step = 1; step <= n; ++step) {
    collected c = {0}; mulib_http_message_t m = {0};
    mulib_http_stream_t *s = mulib_http_stream_create (&o, collect, &c); assert (s);
    for (size_t i = 0; i < n;) {
      size_t k = n - i < step ? n - i : step;
      assert (mulib_http_stream_feed (s, wire + i, k) == 1); i += k;
    }
    assert (mulib_http_stream_finish (s, &m) == 1 && c.headers == 1);
    assert (c.body.length == length && (!length || !memcmp (c.body.data, body, length)));
    assert (m.body_length == 0 && m.trailer_count == (size_t)trailers);
    mulib_http_message_free (&m); mulib_http_stream_destroy (s); mulib_buffer_free (&c.body);
  }
}
static void invalid (const char *wire, int expected)
{
  collected c = {0}; mulib_http_message_t m = {0};
  mulib_http_options_t o = {0, 0, 10, MULIB_HTTP_CHUNK_METADATA};
  mulib_http_stream_t *s = mulib_http_stream_create (&o, collect, &c);
  int rc = 1;
  for (size_t i = 0; i < strlen (wire) && rc == 1; ++i) rc = mulib_http_stream_feed (s, wire + i, 1);
  if (rc == 1) rc = mulib_http_stream_finish (s, &m);
  assert (rc == expected);
  assert (mulib_http_stream_finish (s, &m) == expected);
  mulib_http_stream_destroy (s); mulib_buffer_free (&c.body);
}
int main (void)
{
  const char fixed[] = "HTTP/1.1 103 Early Hints\r\nLink: </x>\r\n\r\nHTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\na\0bc";
  const char chunked[] = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nTrailer: Digest\r\n\r\n1;ext=\"ok\"\r\na\r\n3\r\n\0bc\r\n0\r\nDigest: synthetic\r\n\r\n";
  const char eof[] = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nabc";
  valid (fixed, sizeof fixed - 1, "a\0bc", 4, 0);
  valid (chunked, sizeof chunked - 1, "a\0bc", 4, 1);
  valid (eof, sizeof eof - 1, "abc", 3, 0);
  valid ("HTTP/1.1 204 No Content\r\n\r\n", 27, "", 0, 0);
  invalid ("HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\nabc", -1);
  invalid ("HTTP/1.1 200 OK\r\nContent-Length: 1\r\n\r\nab", -1);
  invalid ("HTTP/1.1 200 OK\r\nContent-Length: 11\r\n\r\n", -2);
  invalid ("HTTP/1.1 200 OK\r\nContent-Length: 1\r\nTransfer-Encoding: chunked\r\n\r\n", -1);
  invalid ("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n1\r\na\r\n0\r\n", -1);
  invalid ("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n1\r\naX\r\n0\r\n\r\n", -1);
  invalid ("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n0\r\nContent-Length: 0\r\n\r\n", -1);
  invalid ("HTTP/1.1 205 Reset Content\r\n\r\nx", -1);
  invalid ("HTTP/1.1 101 Switching Protocols\r\n\r\n", -1);
  invalid ("HTTP/1.1 200 OK\n\n", -1);
  return 0;
}
