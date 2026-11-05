#define MULIB_IMPL_DFA
#define MULIB_IMPL_FST
#define MULIB_IMPL_FST_SERIALIZE
#define MULIB_IMPL_VEC
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_ENDIAN
#define MULIB_IMPL_CRC32
#include "mulib-dfa.h"
#include <assert.h>
int main(void) {
  const unsigned char accepting[] = {0, 0, 1};
  const mulib_nfa_edge_t edges[] = {{0, 1, 0, 0, 1}, {1, 2, 'a', 'z', 0}, {2, 2, 'a', 'z', 0}};
  mulib_nfa_t nfa = {3, 0, accepting, edges, 3}; int status;
  uint32_t word[] = {'h', 'e', 'l', 'l', 'o'};
  mulib_dfa_t *dfa = mulib_dfa_from_nfa(&nfa, NULL, &status);
  assert(dfa && !status && !mulib_dfa_accepts(dfa, word, 5, 0));
  assert(mulib_dfa_accepts(dfa, NULL, 0, 0) == MULIB_FST_NOMATCH);
  mulib_dfa_destroy(dfa); return 0;
}
