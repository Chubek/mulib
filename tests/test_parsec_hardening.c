#define MULIB_IMPL_PARSEC
#include "mulib-parsec.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
int main (void)
{
  mulib_parsec_result_t r;
  mulib_parsec_options_t options = {100000, 1000000, 100000};
  mulib_parsec_parser_t *chain = calloc (20000, sizeof (*chain));
  mulib_parsec_parser_t literal = mulib_parsec_literal ("abcdefgh", 8);
  mulib_parsec_parser_t choices[2], root, empty = mulib_parsec_empty ();
  assert (chain);
  chain[19999] = mulib_parsec_byte ('x');
  for (size_t i = 19999; i; --i) chain[i-1] = mulib_parsec_capture (&chain[i], i);
  assert (mulib_parsec_run (chain, "x", 1, &options, &r) == 1);
  assert (r.count == 19999 && r.captures[r.count-1].first == 0);
  mulib_parsec_result_free (&r);
  options.max_depth = 19000;
  assert (mulib_parsec_run (chain, "x", 1, &options, &r) == -1);
  assert (r.error.code == MULIB_PARSEC_LIMIT && !r.count && !r.captures);
  free (chain);
  assert (mulib_parsec_run_bounded (&literal, "abcdefgh", 8, NULL, 8, &r) == -1);
  assert (r.error.code == MULIB_PARSEC_LIMIT && r.error.offset == 7);
  assert (mulib_parsec_run_bounded (&literal, "abcdefgh", 8, NULL, 9, &r) == 1);
  mulib_parsec_result_free (&r);
  choices[0] = literal; choices[1] = empty; root = mulib_parsec_choice (choices, 2);
  options.max_steps = 1;
  assert(mulib_parsec_run(&literal,"abcdefgh",8,&options,&r)==1);
  mulib_parsec_result_free(&r);
  assert (mulib_parsec_run_bounded (&root, "abcdefgz", 8, NULL, 5, &r) == -1);
  assert (r.error.code == MULIB_PARSEC_LIMIT);
  root = mulib_parsec_sequence (&empty, SIZE_MAX);
  assert (mulib_parsec_run (&root, "", 0, NULL, &r) == -1 && r.error.code == MULIB_PARSEC_INVALID);
  root = mulib_parsec_lookahead (&choices[0], 0);
  assert (mulib_parsec_run_bounded (&root, "abcdefgh", 8, NULL, 5, &r) == -1);
  assert (r.error.code == MULIB_PARSEC_LIMIT);
  return 0;
}
