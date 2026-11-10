#define MULIB_IMPL_POSTINGS
#define MULIB_IMPL_BOOLEANQUERY
#define MULIB_IMPL_POSITIONS
#define MULIB_IMPL_PHRASEQUERY
#define MULIB_IMPL_BM25
#define MULIB_IMPL_TFIDF
#define MULIB_IMPL_TOPK
#define MULIB_IMPL_HYBRIDSEARCH
#include "mulib-booleanquery.h"
#include "mulib-phrasequery.h"
#include "mulib-bm25.h"
#include "mulib-tfidf.h"
#include "mulib-topk.h"
#include "mulib-hybridsearch.h"
#include <assert.h>
#include <float.h>

int
main (void)
{
  mulib_postings_t a = { 0 }, b = { 0 }, out = { 0 };
  for (int i = 99; i >= 0; i--)
    {
      if (i % 2 == 0)
        assert (mulib_postings_put (&a, (uint64_t)i, 1));
      if (i % 3 == 0)
        assert (mulib_postings_put (&b, (uint64_t)i, 2));
    }
  for (int op = 0; op < 3; op++)
    {
      assert (mulib_booleanquery_apply (&a, &b, (mulib_boolean_op_t)op, &out));
      size_t count = 0;
      for (uint64_t i = 0; i < 100; i++)
        {
          int match = op == 0   ? (i % 6 == 0)
                      : op == 1 ? (i % 2 == 0 || i % 3 == 0)
                                : (i % 2 == 0 && i % 3 != 0);
          assert (mulib_postings_find (&out, i, NULL) == match);
          count += (size_t)match;
        }
      assert (out.count == count);
      mulib_postings_free (&out);
    }
  assert (mulib_postings_put (&a, 0, 7));
  uint32_t f;
  assert (mulib_postings_find (&a, 0, &f) && f == 7 && a.count == 50);
  assert (mulib_postings_remove (&a, 0) && !mulib_postings_remove (&a, 0));
  assert (!mulib_booleanquery_apply (&a, &b, (mulib_boolean_op_t)-1, &out));
  out.capacity = 1;
  assert (!mulib_booleanquery_apply (&a, &b, MULIB_BOOLEAN_OR, &out));
  out.capacity = 0;
  mulib_postings_free (&a);
  mulib_postings_free (&b);
  mulib_positions_t p[3] = { { 0 } };
  assert (mulib_positions_add (&p[0], UINT32_MAX));
  assert (mulib_positions_add (&p[1], 0));
  assert (!mulib_phrasequery_match (p, 2));
  for (unsigned i = 0; i < 3; i++)
    assert (mulib_positions_add (&p[i], 10 + i));
  assert (mulib_positions_add (&p[0], 10) && p[0].count == 2);
  assert (mulib_phrasequery_match (p, 3));
  for (unsigned i = 0; i < 3; i++)
    mulib_positions_free (&p[i]);
  double expected = log (1 + (100 - 10 + 0.5) / (10 + 0.5));
  assert (fabs (mulib_bm25_score (1, 10, 20, 20, 100, 1.2, 0.75) - expected) < 1e-12);
  assert (isfinite (mulib_bm25_idf (DBL_MAX, 0)));
  assert (isfinite (mulib_bm25_score (DBL_MAX, 10, 20, 20, 100, 1.2, 0.75)));
  assert (isnan (mulib_bm25_score (0, 101, 0, 1, 100, 1, 1)));
  double tf[] = { 1, 0 }, len[] = { 20, 10 }, avg[] = { 20, 10 }, weight[] = { 1, 1 },
         norm[] = { 0.75, 0.75 };
  assert (
      fabs (mulib_bm25f_score (tf, len, avg, weight, norm, 2, 10, 100, 1.2) - expected)
      < 1e-12);
  tf[0] = 0;
  assert (isnan (mulib_bm25f_score (tf, len, avg, weight, norm, 2, 101, 100, 1.2)));
  assert (mulib_tfidf_idf (10, 10) == 1 && mulib_tfidf_score (3, 10, 10, 0) == 3);
  assert (isnan (mulib_tfidf_score (0, 1, 2, 0)));
  mulib_topk_entry_t entries[] = { { 9, 1 }, { 3, 2 }, { 2, 2 }, { 7, -1 } },
                     best[4] = { { 99, 99 } };
  assert (mulib_topk_select (entries, 4, best, 2) == 2 && best[0].id == 2
          && best[1].id == 3);
  entries[3].score = NAN;
  best[0].id = 99;
  assert (mulib_topk_select (entries, 4, best, 2) == SIZE_MAX && best[0].id == 99);
  uint64_t lex[] = { 1, 1, 2 }, vec[] = { 2, 3 };
  mulib_hybridsearch_hit_t hits[3] = { { 99, 99 } };
  assert (mulib_hybridsearch_rrf (lex, 3, vec, 2, 0, hits, 2) == SIZE_MAX
          && hits[0].id == 99);
  assert (mulib_hybridsearch_rrf (lex, 3, vec, 2, 0, hits, 3) == 3);
  assert (hits[0].id == 2 && fabs (hits[0].score - 4.0 / 3) < 1e-12 && hits[1].id == 1
          && hits[1].score == 1);
  assert (mulib_hybridsearch_rrf (NULL, 0, NULL, 0, 0, NULL, 0) == 0);
  return 0;
}
