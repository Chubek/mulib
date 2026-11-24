/* MuLIB: Length-aware byte tokenizer with ASCII lowercasing into caller buffers.
 * C11 single-header; define MULIB_IMPL_TOKENIZER once. */
#ifndef MULIB_TOKENIZER_H
#define MULIB_TOKENIZER_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct
  {
    const unsigned char *text;
    size_t length, pos;
  } mulib_tokenizer_t;
  void mulib_tokenizer_init (mulib_tokenizer_t *t, const void *text, size_t length);
  /* Returns 1 token, 0 end, -1 invalid. Buffer receives lowercase ASCII with NUL. */
  int mulib_tokenizer_next (mulib_tokenizer_t *t, char *out, size_t capacity,
                            size_t *token_length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_TOKENIZER) && !defined(MULIB_TOKENIZER_IMPLEMENTED)
#define MULIB_TOKENIZER_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifdef __cplusplus
extern "C"
{
#endif
  static int
  mulib_tok_word_ (unsigned c)
  {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
           || c >= 128;
  }
  void
  mulib_tokenizer_init (mulib_tokenizer_t *t, const void *text, size_t len)
  {
    if (!t)
      return;
    t->text = (const unsigned char *)text;
    t->length = text ? len : 0;
    t->pos = 0;
  }
  int
  mulib_tokenizer_next (mulib_tokenizer_t *t, char *out, size_t cap,
                        size_t *token_length)
  {
    if (!t || !out || !cap || (!t->text && t->length))
      return -1;
    size_t p = t->pos;
    while (p < t->length && !mulib_tok_word_ (t->text[p]))
      p++;
    if (p == t->length)
      {
        t->pos = p;
        return 0;
      }
    size_t start = p;
    while (p < t->length && mulib_tok_word_ (t->text[p]))
      p++;
    size_t n = p - start;
    if (n >= cap)
      return -1;
    for (size_t i = 0; i < n; i++)
      {
        unsigned c = t->text[start + i];
        out[i] = (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);
      }
    out[n] = 0;
    t->pos = p;
    if (token_length)
      *token_length = n;
    return 1;
  }
#ifdef __cplusplus
}
#endif
#endif
