#define MULIB_IMPL_TOKENIZER
#define MULIB_IMPL_STOPWORDS
#define MULIB_IMPL_TEXTANALYZER
#include "mulib-textanalyzer.h"
#include <assert.h>
int
main (void)
{
  const char *words[] = { "the" };
  mulib_stopwords_t stop = { words, 1 };
  mulib_textanalyzer_t a;
  char token[32];
  size_t n;
  mulib_textanalyzer_init (&a, "The Quick Fox", 13, &stop);
  assert (mulib_textanalyzer_next (&a, token, sizeof (token), &n) == 1
          && !strcmp (token, "quick"));
  assert (mulib_textanalyzer_next (&a, token, sizeof (token), &n) == 1
          && !strcmp (token, "fox"));
  return 0;
}
