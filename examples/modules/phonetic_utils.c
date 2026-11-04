#define MULIB_IMPL_PHONETIC_UTILS
#include "mulib-phonetic_utils.h"
#include <assert.h>
#include <string.h>
int main(void) {
  char a[5], b[5];
  assert(mulib_phonetic_soundex("Robert", a) && mulib_phonetic_soundex("Rupert", b));
  assert(!strcmp(a, b) && !strcmp(a, "R163")); return 0;
}
