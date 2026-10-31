/* Fail each allocation in turn, including shared JSON/HTTP/editor dependencies. */
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
static size_t calls, fail_at = SIZE_MAX;
static void *fail_malloc (size_t n)
{ return ++calls == fail_at ? NULL : malloc (n); }
static void *fail_calloc (size_t n, size_t size)
{ return ++calls == fail_at ? NULL : calloc (n, size); }
static void *fail_realloc (void *p, size_t n)
{ return ++calls == fail_at ? NULL : realloc (p, n); }
#define malloc fail_malloc
#define calloc fail_calloc
#define realloc fail_realloc
#include "../examples/prowse-common.h"
#undef malloc
#undef calloc
#undef realloc

typedef struct stream
{ size_t at; int opened, closed; } stream;
static int open_stream (void *user, const char *host, unsigned port, int tls, unsigned ms)
{ stream *s = (stream *)user; (void)host; (void)port; (void)tls; (void)ms; s->at = 0; ++s->opened; return 0; }
static int read_stream (void *user, void *data, size_t cap, size_t *got, unsigned ms)
{
  static const char response[] = "HTTP/1.1 200 OK\r\nContent-Length: 12\r\n\r\n{\"value\":42}";
  stream *s = (stream *)user;
  size_t n = sizeof (response) - 1 - s->at;
  (void)ms;
  if (!n) return MULIB_PROWSE_CLOSED;
  if (n > cap) n = cap;
  memcpy (data, response + s->at, n); *got = n; s->at += n; return 0;
}
static int write_stream (void *user, const void *data, size_t n, size_t *sent, unsigned ms)
{ (void)user; (void)data; (void)ms; *sent = n; return 0; }
static void close_stream (void *user)
{ ++((stream *)user)->closed; }
static int count_completion (void *user, const char *text, const char *description)
{ (void)text; (void)description; ++*(size_t *)user; return 0; }

static size_t profile (size_t failure)
{
  const char *json = "{\"version\":1,\"items\":[{\"text\":\"alpha\"},{\"text\":\"beta\"}]}";
  mulib_readline_t *editor;
  size_t attempts, count = 0;
  int rc;
  fail_at = SIZE_MAX; editor = mulib_readline_create (NULL); assert (editor);
  assert (!mulib_readline_completion (editor, "existing", "retain"));
  assert (!mulib_readline_set (editor, "draft", 5));
  calls = 0; fail_at = failure;
  rc = mulib_readline_load_memory (editor, MULIB_READLINE_COMPLETIONS, json, strlen (json), NULL);
  attempts = calls; fail_at = SIZE_MAX;
  assert (!strcmp (mulib_readline_text (editor, NULL, NULL), "draft"));
  assert (!mulib_readline_set (editor, "", 0));
  assert (!mulib_readline_complete (editor, count_completion, &count, NULL));
  assert (count == (rc ? 1u : 3u));
  mulib_readline_destroy (editor); return attempts;
}
static size_t evaluate (size_t failure)
{
  stream s = { 0 };
  mulib_prowse_io_t io = { &s, open_stream, read_stream, write_stream, close_stream, NULL };
  mulib_prowse_protocol_t *p;
  mulib_prowse_control_t *c;
  mulib_json_t *value = NULL;
  size_t attempts;
  int rc;
  fail_at = SIZE_MAX;
  p = mulib_prowse_protocol_connect (MULIB_PROWSE_WEBDRIVER, "http://127.0.0.1:4444", NULL, &io, &rc); assert (p);
  c = mulib_prowse_control_create (p, NULL); assert (c);
  assert (!mulib_prowse_control_attach (c, "test"));
  calls = 0; fail_at = failure;
  rc = mulib_prowse_control_evaluate (c, "6*7", &value);
  attempts = calls; fail_at = SIZE_MAX;
  assert (s.opened == s.closed);
  if (!rc) assert (value && mulib_json_numeric (value) == 42);
  else assert (rc < 0 && !value);
  mulib_json_destroy (value); mulib_prowse_control_destroy (c); mulib_prowse_protocol_destroy (p); return attempts;
}
int main (void)
{
  size_t a = profile (SIZE_MAX), b = evaluate (SIZE_MAX);
  for (size_t i = 1; i <= a; ++i) (void)profile (i);
  for (size_t i = 1; i <= b; ++i) (void)evaluate (i);
  for (size_t i = 1; i <= 2; ++i)
    {
      mulib_readline_t *editor;
      calls = 0; fail_at = i; editor = mulib_readline_create (NULL); assert (!editor);
    }
  fail_at = SIZE_MAX;
  printf ("prowse allocation failure sweeps: %zu profile and %zu request sites passed\n", a, b); return 0;
}
