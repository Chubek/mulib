/** @file mulib-corpus.h
 * @brief Owned token-frequency corpus backed by the MuLIB byte trie.
 * Define MULIB_IMPL_CORPUS and MULIB_IMPL_TRIE.
 */
#ifndef MULIB_CORPUS_H
#define MULIB_CORPUS_H
#include "mulib-trie.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_corpus { mulib_trie_t tokens; uint64_t total; } mulib_corpus_t;
/** Initialize an uninitialized corpus; zero initialization is also valid. */
void mulib_corpus_init(mulib_corpus_t *corpus);
/** Add count occurrences of a binary token; count must be positive.
 * Return 0 on invalid input, overflow or OOM without changing counts. */
int mulib_corpus_add(mulib_corpus_t *corpus, const void *token, size_t length, uint64_t count);
/** Tokenize ASCII letter runs, fold to lowercase and add one per token.
 * Return 0 on failure; already-added tokens remain (incremental operation). */
int mulib_corpus_ingest(mulib_corpus_t *corpus, const char *text);
/** Frequency or zero when absent/invalid. No automatic folding. */
uint64_t mulib_corpus_count(const mulib_corpus_t *corpus, const void *token, size_t length);
/** Release all tokens and reset. */
void mulib_corpus_free(mulib_corpus_t *corpus);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CORPUS) && !defined(MULIB_CORPUS_IMPLEMENTED)
#define MULIB_CORPUS_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
void mulib_corpus_init(mulib_corpus_t *c) { if (c) { mulib_trie_init(&c->tokens); c->total = 0; } }
uint64_t mulib_corpus_count(const mulib_corpus_t *c, const void *s, size_t n)
{ uint64_t v = 0; if (c) (void)mulib_trie_get(&c->tokens, s, n, &v); return v; }
int mulib_corpus_add(mulib_corpus_t *c, const void *s, size_t n, uint64_t count)
{
  uint64_t old;
  if (!c || (!s && n) || !count || c->total > UINT64_MAX - count) return 0;
  old = mulib_corpus_count(c, s, n);
  if (old > UINT64_MAX - count || !mulib_trie_set(&c->tokens, s, n, old + count)) return 0;
  c->total += count; return 1;
}
static int mulib_corpus_letter_(unsigned char c)
{ return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
int mulib_corpus_ingest(mulib_corpus_t *c, const char *text)
{
  char *copy;
  size_t n, i = 0;
  if (!c || !text) return 0;
  n = strlen(text);
  if (n == SIZE_MAX) return 0;
  copy = (char *)malloc(n + 1);
  if (!copy) return 0;
  memcpy(copy, text, n + 1);
  while (i < n) {
    size_t start;
    while (i < n && !mulib_corpus_letter_((unsigned char)copy[i])) ++i;
    start = i;
    while (i < n && mulib_corpus_letter_((unsigned char)copy[i])) {
      if (copy[i] >= 'A' && copy[i] <= 'Z') copy[i] += 'a' - 'A';
      ++i;
    }
    if (i > start && !mulib_corpus_add(c, copy + start, i - start, 1)) { free(copy); return 0; }
  }
  free(copy); return 1;
}
void mulib_corpus_free(mulib_corpus_t *c) { if (c) { mulib_trie_free(&c->tokens); c->total = 0; } }
#ifdef __cplusplus
}
#endif
#endif
