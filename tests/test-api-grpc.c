#define MULIB_IMPL_CDD_GRPC
#include "fixtures/muw3-setup.h"
#include "mulib-grpc.h"
#include <assert.h>
#include <stdio.h>

#define URL "http://fixture.test/Example/Unary"
#define TRAILER(s) "\200\0\0\0" s

static void frame_tests(void)
{
  mulib_buffer_t b = {0};
  mulib_cdd_grpc_frame_t frame = {-9, NULL, 45};
  size_t consumed = 99;
  assert(mulib_cdd_grpc_frame_read(NULL, 0, 0, &frame, &consumed) == 0);
  assert(mulib_cdd_grpc_frame_read(NULL, 1, 0, &frame, &consumed) == -1);
  assert(mulib_cdd_grpc_frame_read("\200\0\0\0\0", 5, 0, &frame, &consumed) == -1);
  assert(mulib_cdd_grpc_frame_read("\0\377\377\377\377", 5, 0, &frame, &consumed) == -2);
  assert(frame.compressed == -9 && frame.length == 45 && consumed == 99);
  assert(mulib_cdd_grpc_frame_write(&b, 0, "x\0y", 3));
  assert(mulib_cdd_grpc_frame_write(&b, 1, b.data + 5, 3));
  assert(!mulib_cdd_grpc_frame_write(&b, 2, "z", 1));
  assert(!mulib_cdd_grpc_frame_write(&b, 0, NULL, 1));
  assert(b.length == 16);
  assert(mulib_cdd_grpc_frame_read(b.data, b.length, 0, &frame, &consumed) == 1);
  assert(consumed == 8 && !frame.compressed && frame.length == 3 &&
      !memcmp(frame.data, "x\0y", 3));
  assert(mulib_cdd_grpc_frame_read(b.data + 8, 7, 0, &frame, &consumed) == 0);
  assert(mulib_cdd_grpc_frame_read(b.data + 8, 8, 2, &frame, &consumed) == -2);
  assert(frame.data == b.data + 5 && consumed == 8);
  assert(mulib_cdd_grpc_frame_read(b.data + 8, 8, 3, &frame, NULL) == 1);
  assert(frame.compressed && frame.length == 3 && !memcmp(frame.data, "x\0y", 3));
  assert(mulib_cdd_grpc_frame_write(&b, 0, NULL, 0));
  assert(mulib_cdd_grpc_frame_read(b.data + 16, 5, 0, &frame, &consumed) == 1);
  assert(frame.length == 0 && consumed == 5);
  while (b.length < 130) {
    size_t length = b.length, cap = b.capacity;
    assert(mulib_cdd_grpc_frame_write(&b, 0, b.data, length));
    assert(!memcmp(b.data, b.data + length + 5, length));
    if (b.length > cap) assert(b.capacity > cap);
  }
  mulib_buffer_free(&b);
}

typedef struct fixture {
  const char *headers;
  const void *body;
  size_t length;
  unsigned status, calls;
  int chunked, fail;
} fixture_t;

static int exchange(void *user, const mulib_uri_t *endpoint, const void *wire,
    size_t n, mulib_webclient_receive_fn receive, void *receiver)
{
  fixture_t *f = (fixture_t *)user;
  mulib_http_message_t request = {0};
  size_t used = 0;
  char header[512], chunk[32];
  int len;
  (void)endpoint;
  f->calls++;
  if (f->fail) return 0;
  assert(mulib_http_parse(wire, n, 0, NULL, &request, &used) == 1 && used == n);
  assert(!strcmp(request.method, "POST") && !strcmp(request.target, "/Example/Unary"));
  assert(!strcmp(mulib_http_header(&request, "Content-Type"), "application/grpc-web+proto"));
  assert(!strcmp(mulib_http_header(&request, "X-Grpc-Web"), "1"));
  assert(!strcmp(mulib_http_header(&request, "Grpc-Accept-Encoding"), "identity"));
  assert(request.body_length == 6 && !memcmp(request.body, "\0\0\0\0\1\177", 6));
  mulib_http_message_free(&request);
  if (f->chunked)
    len = snprintf(header, sizeof(header), "HTTP/1.1 %u Test\r\n%sTransfer-Encoding: chunked\r\n\r\n",
        f->status, f->headers);
  else
    len = snprintf(header, sizeof(header), "HTTP/1.1 %u Test\r\n%sContent-Length: %zu\r\n\r\n",
        f->status, f->headers, f->length);
  assert(len > 0 && (size_t)len < sizeof(header));
  if (!receive(receiver, header, (size_t)len)) return 0;
  if (!f->length) return 1;
  if (f->chunked) {
    len = snprintf(chunk, sizeof(chunk), "%zx\r\n", f->length);
    assert(len > 0 && (size_t)len < sizeof(chunk));
    if (!receive(receiver, chunk, (size_t)len)) return 0;
  }
  if (!receive(receiver, f->body, f->length)) return 0;
  return !f->chunked || receive(receiver, "\r\n0\r\n\r\n", 7);
}

static void decode_bad(const char *body, size_t length, size_t limit, int expected)
{
  mulib_http_header_t headers[] = {{"Content-Type", "application/grpc-web+proto"}};
  mulib_http_message_t response = {0};
  mulib_cdd_grpc_web_reply_t r = {0};
  response.is_response = 1;
  response.status = 200;
  response.headers = headers;
  response.header_count = 1;
  response.body = (unsigned char *)body;
  response.body_length = length;
  r.grpc_status = 47;
  assert(mulib_cdd_grpc_web_decode(&response, limit, &r) == expected);
  assert(r.grpc_status == 47 && !r.message && !r.grpc_message);
}

static void malformed_metadata(const char *fields, size_t length)
{
  char b[256] = {'\200', 0, 0, 0, 0};
  assert(length <= sizeof(b) - 5);
  b[4] = (char)length;
  memcpy(b + 5, fields, length);
  decode_bad(b, length + 5, 1, -1);
}

static void wire_tests(void)
{
  static const char good[] = "\0\0\0\0\1\177" TRAILER("\16") "grpc-status: 0";
  static const char error[] = TRAILER("\52") "Grpc-Status: 16\r\ngrpc-message: A%20B%c3%a9";
  static const char *const bad_headers[] = {
    "Content-Type: application/grpc-web-text+proto\r\n",
    "Content-Type: application/grpc-web+proto nope\r\n",
    "Content-Type: application/grpc-web+proto; type=\"broken\r\n",
    "Content-Type: application/grpc-web+proto, text/plain\r\n",
    "Content-Type: application/grpc-web+proto\r\ncontent-type: application/grpc-web\r\n",
    "Content-Type: application/grpc-web+proto\r\nContent-Encoding: gzip\r\n",
    "Content-Type: application/grpc-web+proto\r\nContent-Encoding: identity\r\ncontent-encoding: identity\r\n",
    "Content-Type: application/grpc-web+proto\r\nGrpc-Encoding: gzip\r\n",
    "Content-Type: application/grpc-web+proto\r\nGrpc-Status: 0\r\n",
    "Content-Type: application/grpc-web+proto\r\nGrpc-Message: stray\r\n",
    ""
  };
  mulib_muw3_client_t c = mulib_muw3_client_default();
  fixture_t f = {"cOnTeNT-tYPe: Application/GRPC-Web+Proto; param=\"a,b\"\r\n"
    "Grpc-Encoding: IDENTITY\r\nContent-Encoding: identity\r\n",
    good, sizeof(good) - 1, 200, 0, 1, 0};
  mulib_cdd_grpc_web_reply_t r = {0};
  c.http = exchange;
  c.http_user = &f;
  assert(mulib_cdd_grpc_web_unary(&c, URL, "\177", 1, 1, &r) == 1);
  assert(r.http.message.chunked && r.grpc_status == 0 && r.message_length == 1);
  assert(r.message[0] == 127 && !r.grpc_message && f.calls == 1);
  mulib_cdd_grpc_web_reply_free(&r);
  mulib_cdd_grpc_web_reply_free(&r);
  assert(mulib_cdd_grpc_web_unary(&c, URL, "aa", 2, 1, &r) == -2 && f.calls == 1);
  f.chunked = 0;
  f.headers = "Content-Type: application/grpc-web\r\n";
  f.body = error;
  f.length = sizeof(error) - 1;
  assert(mulib_cdd_grpc_web_unary(&c, URL, "\177", 1, 1, &r) == 1);
  assert(r.grpc_status == 16 && !r.message && !strcmp(r.grpc_message, "A%20B%c3%a9"));
  mulib_cdd_grpc_web_reply_free(&r);
  f.headers = "Content-Type: application/grpc-web\r\nGrpc-Status: 8\r\nGrpc-Message: limit\r\n";
  f.length = 0;
  assert(mulib_cdd_grpc_web_unary(&c, URL, "\177", 1, 1, &r) == 1);
  assert(r.grpc_status == 8 && !r.message && !strcmp(r.grpc_message, "limit"));
  mulib_cdd_grpc_web_reply_free(&r);
  f.headers = "Content-Type: application/grpc-web\r\nGrpc-Status: 0\r\n";
  assert(mulib_cdd_grpc_web_unary(&c, URL, "\177", 1, 1, &r) == -1);
  f.body = good;
  f.length = sizeof(good) - 1;
  r.grpc_status = -42;
  for (size_t i = 0; i < sizeof(bad_headers) / sizeof(*bad_headers); i++) {
    f.headers = bad_headers[i];
    assert(mulib_cdd_grpc_web_unary(&c, URL, "\177", 1, 1, &r) == -1);
    assert(r.grpc_status == -42 && !r.http.url);
  }
  f.headers = "Content-Type: application/grpc-web\r\n";
  f.status = 204;
  f.length = 0;
  assert(mulib_cdd_grpc_web_unary(&c, URL, "\177", 1, 1, &r) == -1);
  f.fail = 1;
  assert(mulib_cdd_grpc_web_unary(&c, URL, "\177", 1, 1, &r) == MULIB_MUW3_TRANSPORT);

#define BAD(s) decode_bad((s), sizeof(s) - 1, 0, -1)
  BAD("\0\0\0\0\2!");
  BAD("\0\0\0\0\0");
  BAD("\1\0\0\0\0");
  BAD("\201\0\0\0\0");
  BAD(TRAILER("\16") "grpc-status: 0");
  BAD(TRAILER("\16") "grpc-status: 1" "\0\0\0\0\0");
  BAD(TRAILER("\16") "grpc-status: 1" TRAILER("\16") "grpc-status: 2");
  BAD("\0\0\0\0\0\0\0\0\0\0" TRAILER("\16") "grpc-status: 0");
  BAD(TRAILER("\17") "grpc-status: 00");
  BAD(TRAILER("\17") "grpc-status: 17");
  BAD(TRAILER("\17") "grpc-status: +1");
  BAD(TRAILER("\36") "grpc-status: 1\r\ngrpc-status: 2");
  BAD(TRAILER("\20") "grpc-status :1\r\n");
  BAD(TRAILER("\17") "grpc-status: 1\n");
  BAD(TRAILER("\17") "grpc-status: 1\r");
  BAD(TRAILER("\17") "grpc-status: \177\n");
  BAD(TRAILER("\17") "grpc-status:\0001\n");
#undef BAD
#define FIELDS(s) malformed_metadata((s), sizeof(s) - 1)
  FIELDS("grpc-message: missing status");
  FIELDS("grpc-status: 1\r\ngrpc-message: bad%g1");
  FIELDS("grpc-status: 1\r\ngrpc-message: bad%");
  FIELDS("grpc-status: 1\r\ngrpc-message: one\r\nGrpc-Message: two");
  FIELDS("grpc-status: 1\r\ncontent-length: 5");
  FIELDS("grpc-status: 1\r\nno delimiter");
  FIELDS("grpc-status: 1\r\n folded");
  FIELDS("grpc-status: 1\r\n\r\n");
#undef FIELDS
  decode_bad("\0\0\0\0\2", 5, 1, -2);
  decode_bad("\200\0\0\100\1", 5, 1, -2); /* 16 KiB + 1 */
  {
    static const char empty[] = "\0\0\0\0\0" TRAILER("\20") "grpc-status: 0\r\n";
    mulib_http_header_t h[] = {{"Content-Type", "application/grpc-web"}};
    mulib_http_header_t t[] = {{"Grpc-Status", "0"}};
    mulib_http_message_t m = {0};
    m.is_response = 1;
    m.status = 200;
    m.headers = h;
    m.header_count = 1;
    m.body = (unsigned char *)empty;
    m.body_length = sizeof(empty) - 1;
    assert(mulib_cdd_grpc_web_decode(&m, 1, &r) == 1);
    assert(!r.http.url && !r.grpc_status && !r.message_length && r.message);
    mulib_cdd_grpc_web_reply_free(&r);
    m.trailers = t;
    m.trailer_count = 1;
    assert(mulib_cdd_grpc_web_decode(&m, 1, &r) == -1);
  }
}

int main(void)
{
  frame_tests();
  wire_tests();
  puts("gRPC-Web: frame boundaries, HTTP negotiation, unary and status validation passed");
  return 0;
}
