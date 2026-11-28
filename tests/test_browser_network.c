#define MULIB_IMPL_AST
#define MULIB_IMPL_BOUNDED_TEXT
#define MULIB_IMPL_BROWSER_FETCH
#define MULIB_IMPL_BROWSER_ORIGIN
#define MULIB_IMPL_FETCH
#define MULIB_IMPL_FETCH_POLICY
#define MULIB_IMPL_GLOB
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_INTE6
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_URI
#define MULIB_IMPL_UTF8
#include "mulib-browser-fetch.h"
#include <assert.h>
#include <stdlib.h>

static unsigned encoding_fail_at;

static void *
encoding_realloc (void *p, size_t n)
{
  if (encoding_fail_at && --encoding_fail_at == 0)
    return NULL;
  return realloc (p, n);
}

#define realloc encoding_realloc
#include "mulib-web-encoding.h"
#include "mulib-web-textcodec.h"
#undef realloc

typedef struct
{
  unsigned status;
  const char *type;
  const void *body;
  size_t len;
  int result, ignore_budget;
} reply_t;

static int
reply (void *ctx, const char *url, mulib_bounded_text_buffer_t *out,
       unsigned *status, char *type, size_t cap)
{
  const reply_t *r = ctx;
  assert (!strcmp (url, "https://example.com/"));
  assert (out->limit == 4);
  if (r->ignore_budget)
    out->limit = 0;
  *status = r->status;
  if (r->type)
    assert (mulib_bounded_text_copy (type, cap, r->type));
  else
    memset (type, 'x', cap);
  if (r->len && !mulib_bounded_text_append (out, r->body, r->len))
    return 0;
  return r->result;
}

static void
fetch (void)
{
  static const reply_t rejected[] = {
    { 99, "", NULL, 0, 1, 0 },   { 100, "", NULL, 0, 1, 0 },
    { 101, "", NULL, 0, 1, 0 },  { 103, "", NULL, 0, 1, 0 },
    { 199, "", NULL, 0, 1, 0 },  { 600, "", NULL, 0, 1, 0 },
    { 204, "", "x", 1, 1, 0 },   { 205, "", "x", 1, 1, 0 },
    { 304, "", "x", 1, 1, 0 },   { 200, "", "x", 1, 0, 0 },
    { 200, "", "x", 1, -1, 0 },  { 200, "", "hello", 5, 1, 0 },
    { 200, "", "hello", 5, 1, 1 },
    { 200, "\r", NULL, 0, 1, 0 }, { 200, "\n", NULL, 0, 1, 0 },
    { 200, "\v", NULL, 0, 1, 0 }, { 200, "\x7f", "x", 1, 1, 0 },
    { 200, NULL, NULL, 0, 1, 0 }, { 200, "text/json", "x", 1, 1, 0 }
  };
  mulib_browser_fetch_t f = { 0 };
  mulib_bounded_text_buffer_t body = { 0 };
  unsigned status = 77;
  char type[9] = "old";
  reply_t r;
  f.client.transport = reply;
  f.client.ctx = &r;
  f.client.policy.allow_https = 1;
  f.client.policy.max_bytes = 4;
  f.enforce_same_origin = 1;
  assert (mulib_browser_origin_parse ("https://example.com/", &f.initiator));
  body.limit = 7;
  assert (mulib_bounded_text_text (&body, "pre"));
  char *original = body.data;
  for (size_t i = 0; i < sizeof rejected / sizeof *rejected; ++i)
    {
      r = rejected[i];
      assert (!mulib_browser_fetch_get (&f, "https://example.com/", &body,
                                        &status, type, sizeof type));
      assert (status == 77 && !strcmp (type, "old") && body.data == original
              && body.len == 3 && !strcmp (body.data, "pre"));
    }
  const unsigned empty[] = { 204, 205, 304 };
  for (size_t i = 0; i < sizeof empty / sizeof *empty; ++i)
    {
      r = (reply_t){ empty[i], "", NULL, 0, 1, 0 };
      assert (mulib_browser_fetch_get (&f, "https://example.com/", &body,
                                       &status, type, sizeof type));
      assert (status == empty[i] && !type[0] && body.data == original
              && body.len == 3);
    }
  /* Valid field-value HTAB and obs-text, exact body budget and binary bytes. */
  r = (reply_t){ 404, "x\t\x80", "a\0bc", 4, 1, 0 };
  body.limit = 6;
  assert (!mulib_browser_fetch_get (&f, "https://example.com/", &body,
                                    &status, type, sizeof type));
  assert (status == 304 && !type[0] && body.data == original && body.len == 3);
  body.limit = 7;
  assert (mulib_browser_fetch_get (&f, "https://example.com/", &body, &status,
                                   type, sizeof type));
  assert (status == 404 && !strcmp (type, "x\t\x80") && body.len == 7
          && !memcmp (body.data, "prea\0bc", 8));
  mulib_bounded_text_buffer_free (&body);
}

#define R "\xef\xbf\xbd"
#define BYTES(s) { (const unsigned char *)(s), sizeof (s) - 1 }
static void
encodings (void)
{
  static const struct
  {
    muba_span source, output;
  } cases[] = {
    { BYTES (""), BYTES ("") },
    { BYTES ("\xe2\x82"), BYTES (R) },               /* EOF in prefix */
    { BYTES ("\xf0\x90\x80"), BYTES (R) },
    { BYTES ("\xe2\x82!"), BYTES (R "!") },          /* reprocess ASCII */
    { BYTES ("\xe2\xc2\xa2"), BYTES (R "\xc2\xa2") }, /* reprocess lead */
    { BYTES ("\xe0\x9f\xbf"), BYTES (R R R) },        /* overlong < 0800 */
    { BYTES ("\xed\xa0\x80"), BYTES (R R R) },        /* surrogate */
    { BYTES ("\xf0\x8f\xbf\xbf"), BYTES (R R R R) },  /* overlong < 10000 */
    { BYTES ("\xf4\x90\x80\x80"), BYTES (R R R R) },  /* > 10FFFF */
    { BYTES ("\xc0\x80\xff"), BYTES (R R R) },        /* invalid starters */
    { BYTES ("\xf0\x90\x80\0Z"), BYTES (R "\0Z") },
    { BYTES ("\xef\xbb\xbf\xc2\x80\xe0\xa0\x80\xed\x9f\xbf"
             "\xf0\x90\x80\x80\xf4\x8f\xbf\xbf"),
      BYTES ("\xef\xbb\xbf\xc2\x80\xe0\xa0\x80\xed\x9f\xbf"
             "\xf0\x90\x80\x80\xf4\x8f\xbf\xbf") }
  };
  muba_buf out = { 0 };
  for (size_t i = 0; i < sizeof cases / sizeof *cases; ++i)
    {
      assert (mulib_web_textcodec_decode (cases[i].source, &out));
      assert (muba_eq ((muba_span){ out.data, out.len }, cases[i].output));
      if (out.len)
        assert (mulib_utf8_validate (out.data, out.len));
    }
  muba_span cp1252 = BYTES ("\x80\x81\x9f\0\xff");
  muba_span expected = BYTES ("\xe2\x82\xac\xc2\x81\xc5\xb8\0\xc3\xbf");
  assert (mulib_web_encoding_decode (cp1252, 1, &out));
  assert (muba_eq ((muba_span){ out.data, out.len }, expected));
  assert (!mulib_web_encoding_decode ((muba_span){ NULL, 1 }, 0, &out));
  assert (muba_eq ((muba_span){ out.data, out.len }, expected));
  unsigned char longer[64];
  memset (longer, 'a', sizeof longer);
  for (unsigned fail = 1; fail <= 2; ++fail)
    {
      unsigned char *old = out.data;
      encoding_fail_at = fail;
      assert (!mulib_web_encoding_decode ((muba_span){ longer, sizeof longer },
                                         0, &out));
      assert (!encoding_fail_at && out.data == old
              && muba_eq ((muba_span){ out.data, out.len }, expected));
    }
  assert (mulib_web_encoding_decode ((muba_span){ out.data, out.len }, 0,
                                    &out));
  assert (muba_eq ((muba_span){ out.data, out.len }, expected));
  muba_free (&out);
}
#undef BYTES
#undef R

int
main (void)
{
  fetch ();
  encodings ();
  return 0;
}
