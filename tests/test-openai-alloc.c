/* Fail allocations throughout client, routing, SSE, multipart and JSON paths. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
static size_t allocations, fail_at = SIZE_MAX, live;
static void *test_malloc (size_t n)
{ void *p; if (allocations++ == fail_at) return NULL; p = malloc (n); if (p) ++live; return p; }
static void *test_calloc (size_t n, size_t k)
{ void *p; if (allocations++ == fail_at) return NULL; p = calloc (n, k); if (p) ++live; return p; }
static void *test_realloc (void *old, size_t n)
{ void *p; int fresh = old == NULL; if (allocations++ == fail_at) return NULL;
  p = realloc (old, n); if (p && fresh) ++live; return p; }
static void test_free (void *p)
{ if (p) { assert (live); --live; free (p); } }
#define malloc test_malloc
#define calloc test_calloc
#define realloc test_realloc
#define free test_free
#include "fixtures/openai-setup.h"
#undef malloc
#undef calloc
#undef realloc
#undef free
static int transport (void *u, const mulib_uri_t *uri, const void *request, size_t n,
    mulib_webclient_receive_fn receive, void *receiver)
{
  const char response[] = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 11\r\n\r\n{\"ok\":true}";
  (void)u; (void)uri; (void)request; (void)n;
  return receive (receiver, response, sizeof response - 1);
}
static int event (void *u, const mulib_sse_event_t *e)
{ (void)u; (void)e; return 1; }
static int discard (void *u, const void *p, size_t n)
{ (void)u; (void)p; (void)n; return 1; }
static int handle (void *u, const mulib_openai_server_request_t *q, mulib_openai_writer_t *w)
{ (void)u; return mulib_openai_server_json (w, 200, q->json); }
static void operation (unsigned mode)
{
  mulib_openai_client_t c = mulib_openai_client_default ("synthetic");
  mulib_openai_response_t r = {0};
  mulib_openai_request_t q = {0};
  mulib_json_t *v = NULL;
  char *text = NULL;
  c.base_url = "http://fixture/v1"; c.transport.http = transport;
  q.operation_id = "createResponse"; q.body = "{}"; q.body_length = 2; q.content_type = "application/json";
  switch (mode) {
  case 0:
    (void)mulib_openai_client_request (&c, &q, &r); mulib_openai_response_free (&r); break;
  case 1:
    v = mulib_openai_error ("message", "type", NULL, "code"); mulib_json_destroy (v); break;
  case 2:
    v = mulib_openai_chat_request ("model", "hello"); mulib_json_destroy (v); break;
  case 3: {
    mulib_http_header_t p = {(char *)"response_id", (char *)"x/y"};
    (void)mulib_openai_target (mulib_openai_operation ("getResponse"), &p, 1, NULL, 0, 0, &text);
    test_free (text); break;
  }
  case 4: {
    mulib_sse_parser_t *s = mulib_sse_create (0, event, NULL);
    if (s) { const char wire[] = "id: x\nevent: test\ndata: {}\n\n";
      (void)mulib_sse_feed (s, wire, sizeof wire - 1); (void)mulib_sse_finish (s); mulib_sse_destroy (s); }
    break;
  }
  case 5: {
    unsigned char *bytes = NULL; size_t n = 0, cursor = 0;
    mulib_multipart_part_t p = {(char *)"file", (char *)"sample", (char *)"application/octet-stream", "a\0b", 3}, out = {0};
    if (mulib_multipart_encode ("boundary", &p, 1, 0, &bytes, &n) == 1) {
      (void)mulib_multipart_next ("boundary", bytes, n, 0, &cursor, &out);
      mulib_multipart_part_free (&out); test_free (bytes);
    }
    break;
  }
  case 6: {
    const char wire[] = "POST /v1/responses HTTP/1.1\r\nHost: fixture\r\nContent-Type: application/json\r\nContent-Length: 2\r\n\r\n{}";
    mulib_openai_server_t s = mulib_openai_server_default (handle, NULL);
    (void)mulib_openai_server_process (&s, wire, sizeof wire - 1, NULL, discard, NULL);
    break;
  }
  default: assert (0);
  }
}
int main (void)
{
  for (unsigned mode = 0; mode < 7; ++mode) {
    size_t count;
    allocations = 0; fail_at = SIZE_MAX; operation (mode); assert (!live); count = allocations;
    for (size_t i = 0; i < count; ++i) {
      allocations = 0; fail_at = i; operation (mode); assert (!live);
    }
  }
  return 0;
}
