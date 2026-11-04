/* Original reference vectors, exhaustive path checks and deterministic OOM tests. */
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static long allocations_left = -1;
static int allocation_ok(void) { if (allocations_left < 0) return 1; if (!allocations_left) return 0; --allocations_left; return 1; }
static void *checked_malloc(size_t n) { return allocation_ok() ? malloc(n) : NULL; }
static void *checked_calloc(size_t n, size_t s) { return allocation_ok() ? calloc(n, s) : NULL; }
#define malloc checked_malloc
#define calloc checked_calloc
#define MULIB_IMPL_PROBABILITY
#define MULIB_IMPL_CONDITIONAL_PROBABLITY
#define MULIB_IMPL_BAYESIAN_METHODS
#define MULIB_IMPL_BAYSIAN_RANKING
#define MULIB_IMPL_HAMMING_DISTANCE
#define MULIB_IMPL_JACCARD_SIMILARITY
#define MULIB_IMPL_COSINE_SIMILARITY
#define MULIB_IMPL_PEARSON_SIMILARITY
#define MULIB_IMPL_EUCLIDEAN_DISTANCE
#define MULIB_IMPL_LEVENSHTEIN
#define MULIB_IMPL_EDIT_DISTANCE
#define MULIB_IMPL_WEIGHED_EDIT_DISTANE
#define MULIB_IMPL_AUTOMATA_LEVENSHTEIN
#define MULIB_IMPL_FFT
#define MULIB_IMPL_IFFT
#define MULIB_IMPL_LEXOGRAPHIC_SORT
#define MULIB_IMPL_N_GRAMS
#define MULIB_IMPL_PHONETIC_UTILS
#define MULIB_IMPL_TRIE
#define MULIB_IMPL_TRIE_TRAVERSAL
#define MULIB_IMPL_CORPUS
#define MULIB_IMPL_ORTHOGRAPHIC_RULES
#define MULIB_IMPL_MARKOV_CHAIN
#define MULIB_IMPL_HIDDEN_MARKOV_CHAIN
#define MULIB_IMPL_VITERBI_DECODING
#define MULIB_IMPL_BEAM_SEARCH
#define MULIB_IMPL_LINER_REGRESSION
#define MULIB_IMPL_STATS
#define MULIB_IMPL_ANOMALY_DETECTION
#define MULIB_IMPL_KMM
#define MULIB_IMPL_BK_TREES
#define MULIB_IMPL_VANGAGE_POINT_TREES
#define MULIB_IMPL_AST
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_TREE_PARSING
#define MULIB_IMPL_TERM_REWRITING
#include "mulib-bayesian_methods.h"
#include "mulib-conditional_probablity.h"
#include "mulib-baysian_ranking.h"
#include "mulib-hamming_distance.h"
#include "mulib-jaccard_similarity.h"
#include "mulib-cosine_similarity.h"
#include "mulib-pearson_similarity.h"
#include "mulib-euclidean_distance.h"
#include "mulib-edit_distance.h"
#include "mulib-weighed_edit_distane.h"
#include "mulib-automata-levenshtein.h"
#include "mulib-ifft.h"
#include "mulib-lexographic_sort.h"
#include "mulib-n-grams.h"
#include "mulib-phonetic_utils.h"
#include "mulib-trie_traversal.h"
#include "mulib-corpus.h"
#include "mulib-orthographic_rules.h"
#include "mulib-beam_search.h"
#include "mulib-liner_regression.h"
#include "mulib-anomaly-detection.h"
#include "mulib-kmm.h"
#include "mulib-bk-trees.h"
#include "mulib-vangage_point_trees.h"
#include "mulib-tree_parsing.h"
#include "mulib-term_rewriting.h"
#undef malloc
#undef calloc
#include "../library/statistical-text/fixtures.h"
static void near(double a, double b) { assert(fabs(a - b) <= 1e-10 * fmax(1, fabs(b))); }

static void probabilities(void)
{
  double p[] = {DBL_MAX, DBL_MAX}, prior[] = {0.6, 0.4}, ll[] = {log(0.5), log(0.1)}, out[2] = {-1, -1}, z;
  assert(mulib_probability_normalize(p, 2)); near(p[0], 0.5);
  near(mulib_probability_entropy(p, 2), log(2));
  assert(mulib_probability_valid(p, 2, 1e-12));
  p[0] = -1; assert(!mulib_probability_normalize(p, 2) && p[0] == -1);
  assert(!mulib_probability_valid(NULL, 0, 0));
  double zeros[] = {0, 0}; assert(!mulib_probability_normalize(zeros, 2));
  double logs[] = {-1000, -1000}; near(mulib_probability_logsumexp(logs, 2), -1000 + log(2));
  assert(mulib_probability_logsumexp(NULL, 0) == -INFINITY);
  logs[0] = NAN; assert(isnan(mulib_probability_logsumexp(logs, 2)));
  near(mulib_conditional_probability(0.2, 0.4), 0.5);
  near(mulib_conditional_bayes(0.5, 0.6, 0.34), 15.0 / 17);
  assert(isnan(mulib_conditional_probability(0.5, 0.4)));
  assert(isnan(mulib_conditional_bayes(2, 0.5, 1)));
  assert(mulib_bayesian_update(prior, ll, 2, out, &z)); near(out[0], 15.0 / 17); near(exp(z), 0.34);
  ll[0] = ll[1] = -INFINITY;
  assert(!mulib_bayesian_update(prior, ll, 2, out, NULL)); near(out[0], 15.0 / 17);
  ll[0] = ll[1] = -1e300;
  assert(mulib_bayesian_update(prior, ll, 2, out, &z)); near(out[0], 0.6); near(out[1], 0.4);
  assert(mulib_probability_valid(out, 2, 1e-12));
  near(mulib_bayesian_rank_score(3, 4, 1, 1), 2.0 / 3);
  assert(isnan(mulib_bayesian_rank_score(5, 4, 1, 1)));
  double scores[] = {1, 3, 3, 0}; size_t indices[4];
  assert(mulib_bayesian_rank(scores, 4, indices));
  assert(indices[0] == 1 && indices[1] == 2 && indices[2] == 0 && indices[3] == 3);
}
static void distances(void)
{
  double a[] = {1, 2, 3}, b[] = {3, 2, 1}, huge[] = {DBL_MAX, DBL_MAX};
  near(mulib_cosine_similarity(a, b, 3), 5.0 / 7);
  near(mulib_cosine_similarity(huge, huge, 2), 1);
  near(mulib_pearson_similarity(a, b, 3), -1);
  near(mulib_euclidean_distance(a, b, 3), sqrt(8));
  near(mulib_cosine_similarity(NULL, NULL, 0), 1);
  assert(isnan(mulib_pearson_similarity(huge, huge, 2)));
  a[0] = NAN; assert(isnan(mulib_euclidean_distance(a, b, 3)) && isnan(mulib_cosine_similarity(a, b, 3)));
  uint64_t x[] = {1, 1, 2}, y[] = {2, 3, 3};
  near(mulib_jaccard_similarity(x, 3, y, 3), 1.0 / 3);
  near(mulib_jaccard_similarity(NULL, 0, NULL, 0), 1);
  assert(mulib_hamming_distance("abc", "axd", 3) == 2);
  unsigned char h[] = {0, 255}, j[] = {255, 0}; assert(mulib_hamming_bits(h, j, 2) == 16);
  assert(mulib_hamming_distance(NULL, j, 1) == SIZE_MAX);
  assert(mulib_edit_distance("kitten", "sitting") == 3);
  assert(mulib_edit_distance_n("a\0b", 3, "a\0c", 3) == 1);
  assert(mulib_edit_distance_n(NULL, 0, "abc", 3) == 3);
  near(mulib_weighted_edit_distance("ab", 2, "a", 1, 2, 3, 4), 3);
  near(mulib_weighted_edit_distance("a", 1, "ab", 2, 2, 3, 4), 2);
  assert(isnan(mulib_weighted_edit_distance("a", 1, "b", 1, -1, 1, 1)));
  for (size_t n = 0; n < 9; ++n) for (size_t m = 0; m < 9; ++m) {
    char s[9], t[9];
    for (size_t i = 0; i < n; ++i) s[i] = (char)('a' + i % 3);
    for (size_t i = 0; i < m; ++i) t[i] = (char)('a' + i % 2);
    s[n] = t[m] = 0;
    size_t d = mulib_levenshtein_distance(s, t);
    assert(mulib_edit_distance_n(s, n, t, m) == d);
    near(mulib_weighted_edit_distance(s, n, t, m, 1, 1, 1), (double)d);
    for (size_t k = 0; k < 4; ++k) {
      mulib_levenshtein_automaton_t *automaton = mulib_levenshtein_automaton_create(s, n, k);
      assert(automaton);
      for (size_t i = 0; i < m; ++i) assert(mulib_levenshtein_automaton_feed(automaton, t + i, 1));
      assert(mulib_levenshtein_automaton_accepts(automaton) == (d <= k));
      mulib_levenshtein_automaton_reset(automaton);
      assert(mulib_levenshtein_automaton_accepts(automaton) == (n <= k));
      mulib_levenshtein_automaton_destroy(automaton);
    }
  }
}
static void transforms(void)
{
  for (size_t n = 1; n <= 32; n *= 2) {
    mulib_complex_t signal[32], spectrum[32];
    for (size_t i = 0; i < n; ++i) { signal[i].real = (double)(i % 7); signal[i].imag = (double)(i % 3); spectrum[i] = signal[i]; }
    assert(mulib_fft(spectrum, n));
    for (size_t k = 0; k < n; ++k) {
      double re = 0, im = 0;
      for (size_t j = 0; j < n; ++j) {
        double phase = -2 * 3.14159265358979323846 * (double)j * (double)k / n;
        re += signal[j].real * cos(phase) - signal[j].imag * sin(phase);
        im += signal[j].real * sin(phase) + signal[j].imag * cos(phase);
      }
      near(spectrum[k].real, re); near(spectrum[k].imag, im);
    }
    assert(mulib_ifft(spectrum, n));
    for (size_t i = 0; i < n; ++i) { near(spectrum[i].real, signal[i].real); near(spectrum[i].imag, signal[i].imag); }
    assert(!mulib_fft(signal, 3));
    signal[0].real = NAN; assert(!mulib_fft(signal, n));
  }
}
static mulib_hidden_markov_chain_t fixture_model(void)
{
  mulib_hidden_markov_chain_t m = {2, 3, mulib_fixture_initial, mulib_fixture_transition, mulib_fixture_emission}; return m;
}
static void chains(void)
{
  mulib_hidden_markov_chain_t m = fixture_model();
  double logp, best, total = 0, maximum = 0; size_t path[3], beam[3], expected[3] = {0};
  assert(mulib_hidden_markov_chain_forward(&m, mulib_fixture_observations, 3, &logp));
  for (size_t bits = 0; bits < 8; ++bits) {
    size_t a = (bits >> 2) & 1, b = (bits >> 1) & 1, c = bits & 1;
    double p = m.initial[a] * m.emission[a * 3] * m.transition[a * 2 + b] * m.emission[b * 3 + 1]
               * m.transition[b * 2 + c] * m.emission[c * 3 + 2];
    total += p;
    if (p > maximum) { maximum = p; expected[0] = a; expected[1] = b; expected[2] = c; }
  }
  near(exp(logp), total); near(total, 0.03628);
  assert(mulib_viterbi_decode(&m, mulib_fixture_observations, 3, path, &best));
  near(exp(best), maximum); assert(!memcmp(path, expected, sizeof(path)));
  assert(mulib_beam_search(&m, mulib_fixture_observations, 3, 2, beam, &logp));
  near(logp, best); assert(!memcmp(path, beam, sizeof(path)));
  assert(mulib_beam_search(&m, mulib_fixture_observations, 3, 1, beam, &logp)); assert(logp <= best + 1e-12);
  assert(!mulib_beam_search(&m, mulib_fixture_observations, 3, 0, beam, &logp));
  assert(mulib_viterbi_decode(&m, NULL, 0, NULL, &logp)); near(logp, 0);
  size_t invalid[] = {3}; assert(!mulib_hidden_markov_chain_forward(&m, invalid, 1, &logp));
  double impossible_emission[] = {1, 0, 0, 1, 0, 0}; m.emission = impossible_emission;
  size_t impossible[] = {1};
  assert(mulib_hidden_markov_chain_forward(&m, impossible, 1, &logp) && logp == -INFINITY);
  path[0] = 42; assert(!mulib_viterbi_decode(&m, impossible, 1, path, &best) && path[0] == 42);
  mulib_markov_chain_t c = {2, mulib_fixture_transition}; double p[] = {0.6, 0.4};
  assert(mulib_markov_chain_step(&c, p, p)); near(p[0], 0.58);
  assert(mulib_markov_chain_sample(&c, 0, 0.69) == 0 && mulib_markov_chain_sample(&c, 0, 0.7) == 1);
  size_t sequence[] = {0, 1, 0}; double matrix[4];
  assert(mulib_markov_chain_fit(2, sequence, 3, 0, matrix)); near(matrix[1], 1); near(matrix[2], 1);
  assert(!mulib_markov_chain_fit(2, NULL, 0, 0, matrix));
  assert(mulib_markov_chain_fit(2, NULL, 0, 1, matrix)); near(matrix[0], 0.5);
  /* Independently sum all paths for a family of small stochastic models. */
  for (unsigned trial = 1; trial <= 30; ++trial) {
    double initial[] = {(trial % 9 + 1) / 11.0, 0}, transition[4], emission[4];
    initial[1] = 1 - initial[0];
    for (size_t i = 0; i < 2; ++i) {
      transition[2 * i] = ((trial + 3 * i) % 9 + 1) / 11.0;
      transition[2 * i + 1] = 1 - transition[2 * i];
      emission[2 * i] = ((trial + 5 * i) % 9 + 1) / 11.0;
      emission[2 * i + 1] = 1 - emission[2 * i];
    }
    mulib_hidden_markov_chain_t small = {2, 2, initial, transition, emission};
    size_t obs[] = {trial % 2, (trial / 2) % 2, 1, 0}, decoded[4];
    double sum = 0, maximum_path = 0;
    for (unsigned bits = 0; bits < 16; ++bits) {
      size_t previous = bits & 1;
      double probability = initial[previous] * emission[2 * previous + obs[0]];
      for (size_t t = 1; t < 4; ++t) {
        size_t state = (bits >> t) & 1;
        probability *= transition[2 * previous + state] * emission[2 * state + obs[t]]; previous = state;
      }
      sum += probability; maximum_path = fmax(maximum_path, probability);
    }
    assert(mulib_hidden_markov_chain_forward(&small, obs, 4, &logp)); near(exp(logp), sum);
    assert(mulib_viterbi_decode(&small, obs, 4, decoded, &logp)); near(exp(logp), maximum_path);
  }
}
typedef struct visit_state { size_t calls; const char *last; int stop; } visit_state;
static int trie_visit(const void *key, size_t length, uint64_t value, void *ctx)
{
  visit_state *s = (visit_state *)ctx;
  (void)length; (void)value;
  if (s->last) assert(strcmp(s->last, (const char *)key) < 0);
  s->last = (const char *)key; ++s->calls; return !s->stop;
}
/* Trie keys are binary, not NUL-terminated: use separate fixed-size test keys below. */
static int count_visit(const void *key, size_t length, uint64_t value, void *ctx)
{ (void)key; (void)length; (void)value; ++*(size_t *)ctx; return 1; }
static int gram_visit(const unsigned char *gram, size_t n, size_t offset, void *ctx)
{ size_t *count = (size_t *)ctx; assert(n == 2 && offset == *count); assert(gram[0] == (unsigned char)('a' + offset)); ++*count; return 1; }
typedef struct trie_reference { unsigned char present[64]; size_t next, count; } trie_reference;
static int reference_visit(const void *key, size_t length, uint64_t value, void *ctx)
{
  trie_reference *r = (trie_reference *)ctx;
  const unsigned char *k = (const unsigned char *)key;
  while (r->next < 64 && !r->present[r->next]) ++r->next;
  assert(length == 2 && r->next < 64 && k[0] == r->next && k[1] == 0 && value == r->next + 100);
  ++r->next; ++r->count; return 1;
}
static void trie_mutations(void)
{
  mulib_trie_t trie = {0}; trie_reference reference = {{0}, 0, 0};
  uint32_t random = 123;
  for (size_t operation = 0; operation < 4000; ++operation) {
    random = random * UINT32_C(1664525) + UINT32_C(1013904223);
    size_t index = (random >> 16) % 64;
    unsigned char key[] = {(unsigned char)index, 0};
    if (random & 1) { assert(mulib_trie_set(&trie, key, 2, index + 100)); reference.present[index] = 1; }
    else { assert(mulib_trie_remove(&trie, key, 2) == reference.present[index]); reference.present[index] = 0; }
    size_t expected = 0;
    for (size_t i = 0; i < 64; ++i) {
      key[0] = (unsigned char)i; uint64_t value = 0;
      assert(mulib_trie_get(&trie, key, 2, &value) == reference.present[i]);
      if (reference.present[i]) { ++expected; assert(value == i + 100); }
    }
    assert(trie.size == expected); reference.next = reference.count = 0;
    assert(mulib_trie_traverse(&trie, NULL, 0, reference_visit, &reference) == expected && reference.count == expected);
  }
  mulib_trie_free(&trie);
  unsigned char deep[8192] = {0};
  assert(mulib_trie_set(&trie, deep, sizeof(deep), 1));
  assert(mulib_trie_remove(&trie, deep, sizeof(deep)) && !trie.root);
  assert(mulib_trie_set(&trie, deep, sizeof(deep), 2)); mulib_trie_free(&trie);
}
static void text(void)
{
  mulib_trie_t trie = {0}; uint64_t v; size_t length, calls = 0;
  assert(mulib_trie_set(&trie, NULL, 0, 7));
  assert(mulib_trie_set(&trie, "book", 4, 1) && mulib_trie_set(&trie, "books", 5, 2));
  assert(mulib_trie_set(&trie, "boo", 3, 3) && mulib_trie_set(&trie, "a\0b", 3, 4));
  assert(mulib_trie_get(&trie, "a\0b", 3, &v) && v == 4);
  assert(mulib_trie_longest_prefix(&trie, "bookshop", 8, &length, &v) && length == 5 && v == 2);
  assert(mulib_trie_traverse(&trie, "boo", 3, count_visit, &calls) == 3 && calls == 3);
  assert(mulib_trie_remove(&trie, "book", 4) && !mulib_trie_get(&trie, "book", 4, NULL));
  assert(mulib_trie_get(&trie, "books", 5, NULL));
  mulib_trie_free(&trie); assert(!trie.root && !trie.size);
  /* Include terminators in keys when treating them as C strings in callbacks. */
  assert(mulib_trie_set(&trie, "z", 2, 1) && mulib_trie_set(&trie, "a", 2, 1) && mulib_trie_set(&trie, "b", 2, 1));
  visit_state state = {0, NULL, 0}; assert(mulib_trie_traverse(&trie, NULL, 0, trie_visit, &state) == 3);
  state.last = NULL; state.stop = 1; assert(mulib_trie_traverse(&trie, NULL, 0, trie_visit, &state) == 1);
  mulib_trie_free(&trie);
  mulib_corpus_t corpus = {0}; assert(mulib_corpus_ingest(&corpus, mulib_fixture_corpus));
  assert(corpus.total == 8 && corpus.tokens.size == 5 && mulib_corpus_count(&corpus, "rain", 4) == 3);
  mulib_corpus_free(&corpus);
  const char *words[] = {"Zoo", "apple", "Apple", "bee"};
  assert(mulib_lexographic_sort(words, 4, 1) && !strcmp(words[0], "apple") && !strcmp(words[1], "Apple"));
  size_t grams = 0; assert(mulib_ngrams("abcd", 4, 2, gram_visit, &grams) == 3 && grams == 3);
  assert(mulib_ngrams("a", 1, 0, gram_visit, &grams) == SIZE_MAX);
  char code[5]; assert(mulib_phonetic_soundex("Robert", code) && !strcmp(code, "R163"));
  assert(mulib_phonetic_soundex("Ashcraft", code) && !strcmp(code, "A261"));
  assert(mulib_phonetic_soundex("Pfister", code) && !strcmp(code, "P236"));
  assert(!mulib_phonetic_soundex("", code) && !mulib_phonetic_soundex("a-b", code));
  mulib_orthographic_rule_t rules[] = {{MULIB_ORTHOGRAPHIC_SUFFIX, "ies", "y", 1}, {MULIB_ORTHOGRAPHIC_SUFFIX, "s", "", 2}};
  char *word = mulib_orthographic_apply("cities", rules, 2); assert(word && !strcmp(word, "city")); free(word);
  word = mulib_orthographic_apply("is", rules, 2); assert(word && !strcmp(word, "is")); free(word);
}
static int bk_visit(const char *word, size_t distance, void *ctx)
{ size_t *count = (size_t *)ctx; assert(distance == mulib_levenshtein_distance(word, "book")); ++*count; return 1; }
static double point_metric(const void *a, const void *b, void *ctx)
{ (void)ctx; return fabs(*(const double *)a - *(const double *)b); }
static int vp_visit(size_t index, const void *point, double distance, void *ctx)
{ size_t *count = (size_t *)ctx; assert(index < 20 && fabs(*(const double *)point - 5) == distance); ++*count; return 1; }
static void indexes(void)
{
  mulib_bk_tree_t bk = {0}; size_t expected = 0, calls = 0;
  for (size_t i = 0; i < sizeof(mulib_fixture_words) / sizeof(*mulib_fixture_words); ++i) {
    assert(mulib_bk_tree_insert(&bk, mulib_fixture_words[i]));
    expected += mulib_levenshtein_distance(mulib_fixture_words[i], "book") <= 1;
  }
  assert(mulib_bk_tree_insert(&bk, "book") && bk.size == 9);
  assert(mulib_bk_tree_search(&bk, "book", 1, bk_visit, &calls) == expected && calls == expected);
  mulib_bk_tree_free(&bk);
  double points[20], query = 5;
  for (size_t i = 0; i < 20; ++i) points[i] = (double)(i % 13);
  mulib_vp_tree_t vp = {0}; assert(mulib_vp_tree_build(&vp, points, 20, sizeof(double), point_metric, NULL));
  for (size_t radius = 0; radius < 15; ++radius) {
    expected = calls = 0;
    for (size_t i = 0; i < 20; ++i) expected += fabs(points[i] - query) <= radius;
    assert(mulib_vp_tree_search(&vp, &query, (double)radius, vp_visit, &calls) == expected && calls == expected);
  }
  mulib_vp_tree_free(&vp);
}
static void learning(void)
{
  double x[] = {1, 2, 3, 4}, y[] = {3, 5, 7, 9}; mulib_linear_regression_t model;
  assert(mulib_linear_regression_fit(x, y, 4, &model)); near(model.slope, 2); near(model.intercept, 1); near(model.r_squared, 1);
  near(mulib_linear_regression_predict(&model, 5), 11);
  double constant[] = {1, 1}; assert(!mulib_linear_regression_fit(constant, constant, 2, &model));
  mulib_stats_t stats; mulib_stats_init(&stats);
  for (size_t i = 0; i < 4; ++i) assert(mulib_anomaly_train(&stats, x[i]));
  assert(mulib_anomaly_detect(&stats, 100, 3) == 1 && mulib_anomaly_detect(&stats, 2.5, 3) == 0);
  assert(!mulib_anomaly_train(&stats, INFINITY));
  double k[] = {1, 0, 0, 1}, q[] = {1.5, 0.5}, weights[2] = {-1, -1};
  assert(mulib_kmm_fit(k, q, 2, 2, 50, 1e-12, weights) == 1); near(weights[0], 1.5); near(weights[1], 0.5);
  q[0] = 10; q[1] = -10;
  assert(mulib_kmm_fit(k, q, 2, 1.5, 50, 1e-12, weights) == 1); near(weights[0], 1.5); near(weights[1], 0.5);
  assert(!mulib_kmm_fit(k, q, 2, 0.5, 10, 1e-6, weights));
  q[0] = q[1] = DBL_MAX;
  assert(mulib_kmm_fit(k, q, 2, 2, 10, 1e-12, weights) == 1); near(weights[0], 1); near(weights[1], 1);
  q[0] = DBL_MAX; q[1] = -DBL_MAX; weights[0] = weights[1] = -1;
  assert(!mulib_kmm_fit(k, q, 2, 1.5, 10, 1e-12, weights)); assert(weights[0] == -1 && weights[1] == -1);
}
static void terms(void)
{
  size_t offset, steps;
  mulib_ast_t *in = mulib_tree_parse("add(zero,add(zero,x))", 32, &offset);
  mulib_ast_t *lhs = mulib_tree_parse("add(zero,?x)", 32, NULL), *rhs = mulib_tree_parse("?x", 32, NULL), *out = NULL;
  assert(in && lhs && rhs && offset == strlen("add(zero,add(zero,x))"));
  mulib_term_rule_t rule = {lhs, rhs};
  assert(mulib_term_rewrite(in, &rule, 1, 10, &out, &steps) == 1 && steps == 2 && !strcmp(out->production, "x"));
  assert(!strcmp(in->production, "add")); mulib_ast_destroy(out);
  assert(mulib_term_rewrite(in, &rule, 1, 0, &out, &steps) == 2 && steps == 0); mulib_ast_destroy(out);
  assert(!mulib_tree_parse("f(a,)", 32, &offset)); assert(!mulib_tree_parse("a trailing", 32, NULL));
  assert(!mulib_tree_parse("f(a)", 1, NULL)); assert(!mulib_tree_parse("?x(a)", 10, NULL));
  mulib_ast_destroy(in); mulib_ast_destroy(lhs); mulib_ast_destroy(rhs);
  in = mulib_tree_parse("pair(a,b)", 32, NULL); lhs = mulib_tree_parse("pair(?x,?x)", 32, NULL); rhs = mulib_tree_parse("?x", 32, NULL);
  rule.lhs = lhs; rule.rhs = rhs;
  assert(mulib_term_rewrite(in, &rule, 1, 10, &out, &steps) == 1 && steps == 0); mulib_ast_destroy(out);
  mulib_ast_destroy(in); in = mulib_tree_parse("pair(a,a)", 32, NULL);
  assert(mulib_term_rewrite(in, &rule, 1, 10, &out, &steps) == 1 && steps == 1 && !strcmp(out->production, "a")); mulib_ast_destroy(out);
  mulib_ast_destroy(rhs); rhs = mulib_tree_parse("?unbound", 32, NULL); rule.rhs = rhs;
  assert(!mulib_term_rewrite(in, &rule, 1, 10, &out, NULL));
  mulib_ast_destroy(in); mulib_ast_destroy(lhs); mulib_ast_destroy(rhs);
  in = mulib_tree_parse("f(add(zero,a),add(zero,b))", 32, NULL);
  lhs = mulib_tree_parse("add(zero,?x)", 32, NULL); rhs = mulib_tree_parse("?x", 32, NULL);
  rule.lhs = lhs; rule.rhs = rhs;
  assert(mulib_term_rewrite(in, &rule, 1, 10, &out, &steps) == 1 && steps == 2);
  assert(!strcmp(out->child->production, "a") && !strcmp(out->tail->production, "b") && out->tail->parent == out);
  mulib_ast_destroy(out); mulib_ast_destroy(in); mulib_ast_destroy(lhs); mulib_ast_destroy(rhs);
  in = mulib_tree_parse("a", 1, NULL); rule.lhs = rule.rhs = in;
  assert(mulib_term_rewrite(in, &rule, 1, 3, &out, &steps) == 2 && steps == 3);
  mulib_ast_destroy(in); mulib_ast_destroy(out);
}
static void failures(void)
{
  for (long fail = 0; fail < 10; ++fail) {
    mulib_trie_t trie = {0}; assert(mulib_trie_set(&trie, "old", 3, 1));
    allocations_left = fail; int ok = mulib_trie_set(&trie, "other", 5, 2); allocations_left = -1;
    assert(mulib_trie_get(&trie, "old", 3, NULL)); assert(trie.size == (ok ? 2u : 1u));
    assert(mulib_trie_get(&trie, "other", 5, NULL) == ok); mulib_trie_free(&trie);
    allocations_left = fail; mulib_levenshtein_automaton_t *a = mulib_levenshtein_automaton_create("abc", 3, 2); allocations_left = -1;
    mulib_levenshtein_automaton_destroy(a);
    mulib_hidden_markov_chain_t m = fixture_model(); size_t path[3] = {42, 42, 42}; double probability = 42;
    allocations_left = fail; ok = mulib_viterbi_decode(&m, mulib_fixture_observations, 3, path, &probability); allocations_left = -1;
    if (!ok) assert(path[0] == 42 && probability == 42);
    allocations_left = fail; mulib_ast_t *tree = mulib_tree_parse("f(a,g(b))", 16, NULL); allocations_left = -1; mulib_ast_destroy(tree);
    mulib_vp_tree_t vp = {0}; double points[] = {1, 2};
    assert(mulib_vp_tree_build(&vp, points, 2, sizeof(double), point_metric, NULL));
    allocations_left = fail; ok = mulib_vp_tree_build(&vp, points, 1, sizeof(double), point_metric, NULL); allocations_left = -1;
    assert(vp.size == (ok ? 1u : 2u)); mulib_vp_tree_free(&vp);
    mulib_corpus_t corpus = {0}; assert(mulib_corpus_add(&corpus, "old", 3, 1));
    allocations_left = fail; ok = mulib_corpus_add(&corpus, "other", 5, 2); allocations_left = -1;
    assert(corpus.total == (ok ? 3u : 1u) && mulib_corpus_count(&corpus, "old", 3) == 1); mulib_corpus_free(&corpus);
    double prior[] = {0.5, 0.5}, ll[] = {-1, -1}, posterior[] = {42, 42};
    allocations_left = fail; ok = mulib_bayesian_update(prior, ll, 2, posterior, NULL); allocations_left = -1;
    if (!ok) assert(posterior[0] == 42);
    double kernel[] = {1, 0, 0, 1}, q[] = {1, 1}, weights[] = {42, 42};
    allocations_left = fail; ok = mulib_kmm_fit(kernel, q, 2, 2, 10, 1e-8, weights); allocations_left = -1;
    if (!ok) assert(weights[0] == 42);
    mulib_bk_tree_t bk = {0}; assert(mulib_bk_tree_insert(&bk, "old"));
    allocations_left = fail; ok = mulib_bk_tree_insert(&bk, "other"); allocations_left = -1;
    assert(bk.size == (ok ? 2u : 1u)); mulib_bk_tree_free(&bk);
    mulib_ast_t *input = mulib_tree_parse("f(a)", 8, NULL), *output = NULL;
    assert(input);
    allocations_left = fail; ok = mulib_term_rewrite(input, NULL, 0, 1, &output, NULL); allocations_left = -1;
    if (!ok) assert(!output);
    mulib_ast_destroy(input); mulib_ast_destroy(output);
  }
  allocations_left = 0;
  assert(mulib_edit_distance_n("a", 1, "b", 1) == SIZE_MAX);
  assert(isnan(mulib_weighted_edit_distance("a", 1, "b", 1, 1, 1, 1)));
  allocations_left = -1;
}
int main(void)
{
  probabilities(); distances(); transforms(); chains(); text(); trie_mutations(); indexes(); learning(); terms(); failures();
  puts("statistical/text modules: passed"); return 0;
}
