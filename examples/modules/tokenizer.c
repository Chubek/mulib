#define MULIB_IMPL_TOKENIZER
#include "mulib-tokenizer.h"
#include <assert.h>
int
main (void)
{
  mulib_tokenizer_t t;
  char word[16];
  size_t n;
  mulib_tokenizer_init (&t, "Hello, WORLD!", 13);
  assert (mulib_tokenizer_next (&t, word, sizeof (word), &n) == 1 && n == 5
          && !strcmp (word, "hello"));
  assert (mulib_tokenizer_next (&t, word, sizeof (word), &n) == 1
          && !strcmp (word, "world"));
  assert (!mulib_tokenizer_next (&t, word, sizeof (word), &n));
  return 0;
}
