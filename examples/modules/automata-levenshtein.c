#define MULIB_IMPL_AUTOMATA_LEVENSHTEIN
#include "mulib-automata-levenshtein.h"
#include <assert.h>
int main(void) {
  mulib_levenshtein_automaton_t *a = mulib_levenshtein_automaton_create("cat", 3, 1);
  assert(a && mulib_levenshtein_automaton_feed(a, "cart", 4));
  assert(mulib_levenshtein_automaton_accepts(a));
  mulib_levenshtein_automaton_destroy(a); return 0;
}
