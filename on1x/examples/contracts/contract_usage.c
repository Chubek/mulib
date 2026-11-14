/* C-side contract example, NOT a safe On1x foreign facade. Implementation
 * switches occur exactly once. This is a standalone program, not another TU
 * to link alongside on1x_mulib_impl.c. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_JSON
#define MULIB_IMPL_VEC
#include <mulib-json.h>
#include <mulib-vec.h>
int main(void) {
  mulib_json_t *root = mulib_json_new(MULIB_JSON_ARRAY);
  mulib_json_t *child = mulib_json_number(42);
  const mulib_json_t *borrow;
  char *serialized;
  mulib_vec_t vector;
  int value = 42, out;
  mulib_parsec_parser_t letter = mulib_parsec_byte('a');
  mulib_parsec_parser_t capture = mulib_parsec_capture(&letter, 7);
  mulib_parsec_result_t result;
  if (!root || !child || !mulib_json_append(root, child)) {
    /* No successful transfer: child remains our independent owner. */
    mulib_json_destroy(child); mulib_json_destroy(root); return 1;
  }
  child = NULL; /* On success, root owns it. Never drop the child separately. */
  borrow = mulib_json_at(root, 0);
  assert(borrow && mulib_json_numeric(borrow) == 42);
  serialized = mulib_json_stringify(root, 0);
  mulib_json_destroy(root); /* borrow now expires; serialized is independent. */
  if (!serialized) return 1;
  puts(serialized); free(serialized);
  mulib_vec_init(&vector, sizeof value);
  if (!mulib_vec_push(&vector, &value)) { mulib_vec_free(&vector); return 1; }
  assert(mulib_vec_pop(&vector, &out) && out == 42);
  mulib_vec_free(&vector); /* Only byte storage is destroyed. */
  if (mulib_parsec_run(&capture, "a", 1, NULL, &result) != 1) return 1;
  assert(result.count == 1 && result.captures[0].tag == 7);
  mulib_parsec_result_free(&result); /* Does not own the grammar or input. */
  return 0;
}
