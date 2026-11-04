/* Fail each allocation independently and verify transactional outputs/cleanup. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static size_t calls, fail_at = SIZE_MAX, live;
static void *http_malloc (size_t n)
{
  void *p;
  if (calls++ == fail_at) return NULL;
  p = malloc (n); if (p) ++live; return p;
}
static void *http_calloc (size_t n, size_t size)
{
  void *p;
  if (calls++ == fail_at) return NULL;
  p = calloc (n,size); if (p) ++live; return p;
}
static void *http_realloc (void *old, size_t n)
{
  void *p;
  int fresh = old == NULL;
  assert (n);
  if (calls++ == fail_at) return NULL;
  p = realloc (old,n); if (p && fresh) ++live; return p;
}
static void http_free (void *p)
{ if (p) { assert (live); --live; free (p); } }
#define malloc http_malloc
#define calloc http_calloc
#define realloc http_realloc
#define free http_free
#define main http_hardening_baseline_main
#include "test_http_hardening.c"
#undef main
#undef malloc
#undef calloc
#undef realloc
#undef free

static void fail_on (size_t n) { calls = 0; fail_at = n; }
static int operation (int mode)
{
  const char request[] = "POST / HTTP/1.1\r\nHost: example.test\r\nTransfer-Encoding: chunked\r\n\r\n3\r\na\0b\r\n0\r\n\r\n";
  mulib_http_header_t headers[] = {{"Host","example.test"},{"Transfer-Encoding","chunked"}};
  mulib_http_message_t source = {0}, out = {0}, before = {0};
  unsigned char marker, *wire = &marker;
  size_t length = 99, used = 99;
  int rc;
  before.status = 599; out = before;
  source.method = "POST"; source.target = "/"; source.chunked = 1;
  source.headers = headers; source.header_count = 2;
  source.body = (unsigned char *)"a\0b"; source.body_length = 3;
  switch (mode) {
  case 0: rc = mulib_http_parse (request,sizeof (request)-1,0,NULL,&out,&used); break;
  case 1: rc = mulib_http_serialize (&source,NULL,&wire,&length); break;
  case 2: rc = mulib_http_request_create (&out,"POST","/",headers,2,"a\0b",3); break;
  default: rc = mulib_http_response_create (&out,200,"application/octet-stream","a\0b",3); break;
  }
  if (rc == MULIB_HTTP_OK) {
    if (mode == 1) http_free (wire);
    else mulib_http_message_free (&out);
  } else {
    assert (rc == MULIB_HTTP_NOMEM);
    assert (wire == &marker && length == 99 && used == (mode ? 99u : 0u));
    assert (!memcmp (&out,&before,sizeof (out)));
  }
  return rc;
}
static void framing (void)
{
  const unsigned char settings[] = {0,2,0,0,0,1};
  const char prefix[64] = "prefix";
  unsigned char many[66];
  mulib_http2_frame_t h2 = {4,0,0,settings,sizeof (settings)};
  mulib_http3_frame_t h3 = {4,many,sizeof (many)}, parsed, before;
  mulib_buffer_t wire = {0};
  size_t allocations, used;
  for (unsigned i = 0; i < 33; ++i) { many[2*i] = (unsigned char)(i+16); many[2*i+1] = 0; }
  fail_on (SIZE_MAX);
  assert (mulib_http3_frame_write (&wire,&h3));
  memset (&before,0xa5,sizeof (before)); parsed = before; used = 99;
  fail_on (0);
  assert (mulib_http3_frame_read (wire.data,wire.length,100,&parsed,&used) == -3);
  assert (used == 99 && !memcmp (&parsed,&before,sizeof (parsed)));
  fail_on (SIZE_MAX); mulib_buffer_free (&wire); assert (!live);
  for (int mode = 0; mode < 2; ++mode) {
    fail_on (SIZE_MAX);
    assert (mode ? mulib_http3_frame_write (&wire,&h3) : mulib_http2_frame_write (&wire,&h2));
    allocations = calls; mulib_buffer_free (&wire);
    for (size_t i = 0; i < allocations; ++i) {
      fail_on (SIZE_MAX); assert (mulib_buffer_append (&wire,prefix,sizeof (prefix)));
      fail_on (i);
      int ok = mode ? mulib_http3_frame_write (&wire,&h3) : mulib_http2_frame_write (&wire,&h2);
      assert (!ok && wire.length == sizeof (prefix) && !memcmp (wire.data,prefix,sizeof (prefix)));
      mulib_buffer_free (&wire); assert (!live);
    }
  }
}
static void limits_before_allocation (void)
{
  char long_value[129];
  mulib_http_header_t h[] = {{"Host","x"},{"A",long_value},{"B",long_value}};
  mulib_http_message_t m = {0};
  mulib_http_options_t o = {200,100,100,0};
  unsigned char marker, *wire = &marker;
  size_t n = 99;
  memset (long_value,'x',128); long_value[128] = 0;
  m.method = "GET"; m.target = "/"; m.headers = h; m.header_count = 3;
  fail_on (0);
  assert (mulib_http_serialize (&m,&o,&wire,&n) == MULIB_HTTP_LIMIT && !calls);
  assert (wire == &marker && n == 99);
  o.max_header_bytes = 20;
  assert (mulib_http_serialize (&m,&o,&wire,&n) == MULIB_HTTP_LIMIT && !calls);
  fail_on (SIZE_MAX);
}
static void tls_allocations (void)
{
  size_t allocations = 0;
  for (size_t i = 0; i <= allocations; ++i) {
    tls_fixture f = {0};
    mulib_http_message_t response = {0}, before = {0};
    before.status = 599; response = before;
    f.verified = 1; f.reply = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    fail_on (i ? i-1 : SIZE_MAX);
    int rc = mulib_https_request ("https://example.test/","GET",NULL,0,NULL,0,NULL,tls_connect,&f,&response);
    if (!i) {
      assert (rc == MULIB_WEBCLIENT_OK); allocations = calls;
      mulib_http_message_free (&response);
    } else {
      assert (rc == MULIB_WEBCLIENT_NOMEM || rc == MULIB_WEBCLIENT_TRANSPORT);
      assert (!memcmp (&response,&before,sizeof (response)));
    }
    assert (f.closed == f.connected);
    mulib_buffer_free (&f.sent); assert (!live);
  }
}
int main (void)
{
  /* This TU only overrides the allocation functions that these headers use. */
  (void)http_calloc;
  for (int mode = 0; mode < 4; ++mode) {
    size_t allocations;
    fail_on (SIZE_MAX); assert (operation (mode) == MULIB_HTTP_OK);
    allocations = calls; assert (!live);
    for (size_t i = 0; i < allocations; ++i) {
      fail_on (i); assert (operation (mode) == MULIB_HTTP_NOMEM); assert (!live);
    }
  }
  framing (); limits_before_allocation (); tls_allocations ();
  puts ("HTTP allocation failures: transactional outputs and cleanup OK");
  return 0;
}
