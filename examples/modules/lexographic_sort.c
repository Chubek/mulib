#define MULIB_IMPL_LEXOGRAPHIC_SORT
#include "mulib-lexographic_sort.h"
#include <assert.h>
#include <string.h>
int main(void) {
  const char *words[] = {"Zoo", "apple", "Bee"};
  assert(mulib_lexographic_sort(words, 3, 1) && !strcmp(words[0], "apple"));
  return 0;
}
