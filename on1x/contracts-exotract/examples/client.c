/* Link libmulib.a generated for json + vec + parsec; no implementation defines. */
#include "mulib.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  mulib_json_t *root = mulib_json_new(MULIB_JSON_ARRAY);
  mulib_json_t *child = mulib_json_number(42);
  if (!root || !child || !mulib_json_append(root, child)) {
    mulib_json_destroy(child); mulib_json_destroy(root); return 1;
  }
  assert(mulib_json_numeric(mulib_json_at(root, 0)) == 42);
  char *text = mulib_json_stringify(root, 0);
  mulib_json_destroy(root);
  if (!text) return 1;
  puts(text); free(text);
  mulib_vec_t values; int n = 42, result = 0;
  mulib_vec_init(&values, sizeof n);
  assert(mulib_vec_push(&values, &n));
  assert(mulib_vec_pop(&values, &result) && result == 42);
  mulib_vec_free(&values);
  mulib_parsec_parser_t letter = mulib_parsec_byte('a');
  mulib_parsec_result_t parsed;
  assert(mulib_parsec_run(&letter, "a", 1, NULL, &parsed) == 1);
  mulib_parsec_result_free(&parsed);
  return 0;
}
