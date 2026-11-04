#define MULIB_IMPL_ORTHOGRAPHIC_RULES
#include "mulib-orthographic_rules.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
  mulib_orthographic_rule_t rule = {MULIB_ORTHOGRAPHIC_SUFFIX, "ies", "y", 1};
  char *word = mulib_orthographic_apply("cities", &rule, 1);
  assert(word && !strcmp(word, "city")); free(word); return 0;
}
