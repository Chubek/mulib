#define MULIB_IMPL_TOKENIZER
#define MULIB_IMPL_STOPWORDS
#define MULIB_IMPL_TEXTANALYZER
#define MULIB_IMPL_FUZZYQUERY
#define MULIB_IMPL_NDJSON
#define MULIB_IMPL_TSV
#define MULIB_IMPL_INGEST_STREAM
#include "mulib-textanalyzer.h"
#include "mulib-fuzzyquery.h"
#include "mulib-ndjson.h"
#include "mulib-tsv.h"
#include "mulib-ingest-stream.h"
#include <assert.h>

typedef struct
{
  size_t count, length;
  int reject;
  unsigned char bytes[64];
} sink_t;
static int
emit (const void *p, size_t n, void *ctx)
{
  sink_t *s = ctx;
  if (s->reject)
    return 0;
  assert (s->length + n <= sizeof (s->bytes));
  if (n)
    memcpy (s->bytes + s->length, p, n);
  s->length += n;
  s->count++;
  return 1;
}
/* Independent full-matrix oracle for all binary words up to length four. */
static size_t
distance (const char *a, size_t an, const char *b, size_t bn)
{
  size_t d[5][5];
  for (size_t i = 0; i <= an; i++)
    d[i][0] = i;
  for (size_t j = 0; j <= bn; j++)
    d[0][j] = j;
  for (size_t i = 1; i <= an; i++)
    for (size_t j = 1; j <= bn; j++)
      {
        size_t x = d[i - 1][j] + 1, y = d[i][j - 1] + 1,
               z = d[i - 1][j - 1] + (a[i - 1] != b[j - 1]);
        d[i][j] = x < y ? x : y;
        if (z < d[i][j])
          d[i][j] = z;
      }
  return d[an][bn];
}
int
main (void)
{
  const char *words[] = { "a", "the" };
  mulib_stopwords_t stop = { words, 2 };
  mulib_textanalyzer_t a;
  char out[32];
  size_t n = 99;
  mulib_textanalyzer_init (&a, "The QUICK\0fox", 13, &stop);
  assert (mulib_textanalyzer_next (&a, out, 3, &n) == -1 && n == 99);
  assert (mulib_textanalyzer_next (&a, out, sizeof (out), &n) == 1
          && !strcmp (out, "quick"));
  assert (mulib_textanalyzer_next (&a, out, sizeof (out), &n) == 1
          && !strcmp (out, "fox"));
  assert (mulib_textanalyzer_next (&a, out, sizeof (out), &n) == 0);
  assert (!mulib_stopwords_contains (&stop, "The")
          && mulib_stopwords_contains (&stop, "a"));
  for (size_t an = 0; an <= 4; an++)
    for (size_t bn = 0; bn <= 4; bn++)
      for (unsigned x = 0; x < (1u << an); x++)
        for (unsigned y = 0; y < (1u << bn); y++)
          {
            char aa[4], bb[4];
            for (size_t i = 0; i < an; i++)
              aa[i] = (char)('a' + ((x >> i) & 1));
            for (size_t i = 0; i < bn; i++)
              bb[i] = (char)('a' + ((y >> i) & 1));
            size_t d = distance (aa, an, bb, bn);
            for (size_t limit = 0; limit <= 4; limit++)
              assert (mulib_fuzzyquery_distance (aa, an, bb, bn, limit)
                      == (d > limit ? limit + 1 : d));
          }
  assert (mulib_fuzzyquery_distance (NULL, 1, "", 0, 1) == SIZE_MAX);
  mulib_ndjson_t json;
  const char *line;
  mulib_ndjson_init (&json, "\r\n{}\r\n\n[]", 9);
  assert (mulib_ndjson_next (&json, &line, &n) == 1 && n == 2 && !memcmp (line, "{}", 2)
          && json.line == 2);
  assert (mulib_ndjson_next (&json, &line, &n) == 1 && n == 2
          && !memcmp (line, "[]", 2));
  assert (mulib_ndjson_next (&json, &line, &n) == 0);
  mulib_tsv_t t;
  mulib_tsv_init (&t, "\tx\t", 3);
  assert (mulib_tsv_next (&t, &line, &n) == 1 && n == 0);
  assert (mulib_tsv_next (&t, &line, &n) == 1 && n == 1 && *line == 'x');
  assert (mulib_tsv_next (&t, &line, &n) == 1 && n == 0);
  assert (!mulib_tsv_next (&t, &line, &n));
  for (size_t split = 0; split <= 8; split++)
    {
      mulib_ingest_stream_t stream = { 0 };
      sink_t sink = { 0 };
      const char *text = "ab\r\n\nxyz";
      assert (mulib_ingest_stream_init (&stream, 4));
      assert (mulib_ingest_stream_push (&stream, text, split, emit, &sink) != SIZE_MAX);
      assert (mulib_ingest_stream_push (&stream, text + split, 8 - split, emit, &sink)
              != SIZE_MAX);
      assert (mulib_ingest_stream_finish (&stream, emit, &sink) == 1);
      assert (sink.count == 3 && sink.length == 5 && !memcmp (sink.bytes, "abxyz", 5));
      assert (!mulib_ingest_stream_finish (&stream, emit, &sink));
      assert (!mulib_ingest_stream_init (&stream, 4));
      mulib_ingest_stream_free (&stream);
    }
  mulib_ingest_stream_t stream = { 0 };
  sink_t sink = { 0 };
  assert (mulib_ingest_stream_init (&stream, 3));
  assert (mulib_ingest_stream_push (&stream, "abcd", 4, emit, &sink) == SIZE_MAX
          && stream.length == 3);
  sink.reject = 1;
  assert (mulib_ingest_stream_finish (&stream, emit, &sink) == SIZE_MAX
          && stream.length == 3);
  sink.reject = 0;
  assert (mulib_ingest_stream_finish (&stream, emit, &sink) == 1);
  mulib_ingest_stream_free (&stream);
  return 0;
}
