#define MULIB_IMPL_STOPWORDS
#include "mulib-stopwords.h"
#include <assert.h>
int
main (void)
{
  const char *words[] = { "a", "and", "the" };
  mulib_stopwords_t stop = { words, 3 };
  assert (mulib_stopwords_contains (&stop, "the")
          && !mulib_stopwords_contains (&stop, "cat"));
  return 0;
}
