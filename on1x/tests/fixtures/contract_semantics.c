/* Source-contract regression tests, not a general safe-FFI proof. All kernel
 * implementation switches live in this one C11 TU. No LibStone runtime. */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static long failures_after = -1;
static size_t live_allocations;
static int deny_allocation(void) {
  if (failures_after < 0) return 0;
  if (failures_after == 0) return 1;
  --failures_after;
  return 0;
}
static void *test_malloc(size_t n) {
  void *p;
  if (deny_allocation()) return NULL;
  p = malloc(n);
  if (p) ++live_allocations;
  return p;
}
static void *test_calloc(size_t n, size_t size) {
  void *p;
  if (deny_allocation()) return NULL;
  p = calloc(n, size);
  if (p) ++live_allocations;
  return p;
}
static void *test_realloc(void *old, size_t n) {
  void *p;
  int fresh = old == NULL;
  assert(n != 0);
  if (deny_allocation()) return NULL;
  p = realloc(old, n);
  if (p && fresh) ++live_allocations;
  return p;
}
static void test_free(void *p) {
  if (p) { assert(live_allocations); --live_allocations; }
  free(p);
}
#define malloc test_malloc
#define calloc test_calloc
#define realloc test_realloc
#define free test_free
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_JSON
#define MULIB_IMPL_VEC
#include <mulib-json.h>
#include <mulib-vec.h>
#undef malloc
#undef calloc
#undef realloc
#undef free

static void json_contracts(void) {
  mulib_json_t *object, *child, *next, *array, *value;
  mulib_json_error_t error;
  const mulib_json_t *borrow;
  const char bytes[] = {'a', '\0', 'b'};
  const char key[] = {'k', '\0', 'y'};
  const char *text;
  char *printed;
  size_t n, baseline;
  long fail;
  assert(!mulib_json_new((mulib_json_type)99));
  assert(!mulib_json_number(NAN));
  assert(!mulib_json_string(NULL));
  assert(!mulib_json_string_n("x", SIZE_MAX));
  assert(mulib_json_typeof(NULL) == MULIB_JSON_NULL);
  assert(mulib_json_size(NULL) == 0 && mulib_json_boolean(NULL) == 0);
  object = mulib_json_new(MULIB_JSON_OBJECT);
  child = mulib_json_number(42);
  assert(object && child);
  baseline = live_allocations;
  failures_after = 0;
  assert(!mulib_json_set(object, "answer", child));
  failures_after = -1;
  assert(live_allocations == baseline && !mulib_json_get(object, "answer"));
  assert(mulib_json_numeric(child) == 42); /* Failed set did not consume child. */
  assert(mulib_json_set(object, "answer", child));
  baseline = live_allocations;
  assert(mulib_json_set(object, "answer", child)); /* Successful no-op. */
  assert(live_allocations == baseline);
  borrow = mulib_json_get(object, "answer");
  assert(borrow == child && mulib_json_numeric(borrow) == 42);
  next = mulib_json_bool(1);
  assert(next && mulib_json_set(object, "answer", next));
  /* child/borrow are now invalid: deliberately do not read or drop them. */
  assert(mulib_json_boolean(mulib_json_get(object, "answer")));
  assert(!mulib_json_get(object, "absent"));
  value = mulib_json_new(MULIB_JSON_NULL);
  assert(value && mulib_json_set(object, "null", value));
  assert(mulib_json_get(object, "null") != NULL);
  array = mulib_json_new(MULIB_JSON_ARRAY);
  assert(array && mulib_json_set(object, "items", array));
  child = mulib_json_string_n(bytes, sizeof bytes);
  assert(child && mulib_json_append(array, child));
  assert(!mulib_json_append(array, child)); /* Already owned. */
  assert(!mulib_json_append(array, object)); /* Parent cycle. */
  text = mulib_json_text(mulib_json_at(array, 0), &n);
  assert(n == sizeof bytes && memcmp(text, bytes, n) == 0);
  n = 99; assert(!mulib_json_text(object, &n) && n == 0);
  child = mulib_json_string_n(NULL, 0);
  assert(child && mulib_json_set_n(object, key, sizeof key, child));
  assert(mulib_json_get_n(object, key, sizeof key) == child);
  text = mulib_json_key(object, mulib_json_size(object)-1, &n);
  assert(n == sizeof key && memcmp(text, key, n) == 0);
  n = 99; assert(!mulib_json_key(object, SIZE_MAX, &n) && n == 0);
  printed = mulib_json_stringify(object, 0);
  assert(printed && strstr(printed, "\\u0000"));
  mulib_json_destroy(object); /* Printed text is independently owned. */
  assert(strstr(printed, "answer")); test_free(printed);
  mulib_json_destroy(NULL);
  printed = mulib_json_stringify(NULL, 0);
  assert(printed && strcmp(printed, "null") == 0); test_free(printed);
  assert(live_allocations == 0);
  /* Exercise every successive partial-allocation failure, not only the first. */
  for (fail = 0; fail < 100; ++fail) {
    const char source[] = "{\"a\":[1,2,{\"b\":\"text\"}],\"ok\":true}";
    failures_after = fail;
    object = mulib_json_parse(source, sizeof source-1, &error);
    failures_after = -1;
    if (object) {
      assert(!error.message);
      mulib_json_destroy(object);
    } else assert(error.message);
    assert(live_allocations == 0);
  }
  object = mulib_json_parse("{", 1, &error);
  assert(!object && error.message && live_allocations == 0);
  object = mulib_json_parse("[1,2,3]", 7, &error);
  assert(object);
  baseline = live_allocations;
  for (fail = 0; fail < 20; ++fail) {
    failures_after = fail; printed = mulib_json_stringify(object, 1); failures_after = -1;
    test_free(printed); assert(live_allocations == baseline);
  }
  mulib_json_destroy(object);
  assert(live_allocations == 0);
}

static void vec_contracts(void) {
  mulib_vec_t v;
  int value = 42, output = -1;
  size_t i, capacity;
  unsigned char *old;
  void *resource;
  mulib_vec_init(&v, sizeof value);
  assert(mulib_vec_reserve(&v, 1));
  capacity = v.capacity;
  for (i = 0; i < capacity; ++i) assert(mulib_vec_push(&v, &value));
  old = v.data;
  failures_after = 0;
  assert(!mulib_vec_push(&v, mulib_vec_at(&v, 0)));
  failures_after = -1;
  assert(v.data == old && v.length == capacity && v.capacity == capacity);
  assert(mulib_vec_push(&v, mulib_vec_at(&v, 0))); /* Internal alias across growth. */
  assert(*(int *)mulib_vec_at(&v, capacity) == 42);
  assert(!mulib_vec_at(&v, v.length));
  assert(mulib_vec_pop(&v, &output) && output == 42);
  assert(mulib_vec_pop(&v, NULL));
  mulib_vec_clear(&v);
  output = 777; assert(!mulib_vec_pop(&v, &output) && output == 777);
  assert(v.data && v.capacity && v.length == 0);
  mulib_vec_free(&v); mulib_vec_free(&v);
  assert(v.stride == sizeof value && !v.data && !v.length && !v.capacity);
  assert(mulib_vec_push(&v, &value)); /* Free retains stride for reuse. */
  mulib_vec_free(&v);
  resource = test_malloc(16); assert(resource);
  mulib_vec_init(&v, sizeof resource); assert(mulib_vec_push(&v, &resource));
  mulib_vec_clear(&v); mulib_vec_free(&v);
  assert(live_allocations == 1); /* Raw bytes have no nested pointee destructor. */
  test_free(resource);
  mulib_vec_init(&v, 0); assert(!mulib_vec_reserve(&v, 1)); mulib_vec_free(&v);
  assert(live_allocations == 0);
}

static int predicate(unsigned char byte, void *user) {
  return byte == *(const unsigned char *)user; /* Pure borrowed user state. */
}
static void parsec_contracts(void) {
  mulib_parsec_result_t result;
  mulib_parsec_parser_t leaf, captured, parts[3], parser, literal, named;
  mulib_parsec_parser_t alternatives[2], choice, repeated, end, empty, any, set, look, ref;
  const mulib_parsec_parser_t *slot;
  const unsigned char expected = 'a';
  unsigned char bitmap[32] = {0};
  const char bytes[] = "ab";
  literal = mulib_parsec_literal(bytes, 2);
  assert(literal.as.text.bytes == (const unsigned char *)bytes); /* Borrow, not a copy. */
  named = mulib_parsec_named(literal, "pair");
  end = mulib_parsec_end(); empty = mulib_parsec_empty(); any = mulib_parsec_any();
  leaf = mulib_parsec_byte('a');
  captured = mulib_parsec_capture(&leaf, 7);
  parts[0] = captured; parts[1] = mulib_parsec_byte('b'); parts[2] = end;
  parser = mulib_parsec_sequence(parts, 3);
  assert(mulib_parsec_run(&parser, bytes, 2, NULL, &result) == 1);
  assert(result.count == 1 && result.captures[0].tag == 7);
  assert(result.captures[0].offset == 0 && result.captures[0].length == 1);
  mulib_parsec_result_free(&result); mulib_parsec_result_free(&result);
  assert(mulib_parsec_run(&parser, "ac", 2, NULL, &result) == 0);
  assert(!result.count && !result.captures); mulib_parsec_result_free(&result);
  failures_after = 0;
  assert(mulib_parsec_run(&parser, bytes, 2, NULL, &result) == -1);
  failures_after = -1;
  assert(!result.count && !result.captures); mulib_parsec_result_free(&result);
  assert(mulib_parsec_run_bounded(&named, bytes, 2, NULL, 1, &result) == -1);
  assert(!result.count && !result.captures); mulib_parsec_result_free(&result);
  assert(mulib_parsec_run(&leaf, bytes, 2, NULL, &result) == 1 && result.consumed == 1);
  mulib_parsec_result_free(&result); /* Prefix success, not whole-input parsing. */
  alternatives[0] = mulib_parsec_range('0', '9');
  alternatives[1] = mulib_parsec_satisfy(predicate, (void *)&expected, "letter a");
  choice = mulib_parsec_choice(alternatives, 2); repeated = mulib_parsec_repeat(&choice, 1, 3);
  assert(mulib_parsec_run(&repeated, "a1a", 3, NULL, &result) == 1);
  mulib_parsec_result_free(&result);
  bitmap['a'/8] = (unsigned char)(1u << ('a'%8)); set = mulib_parsec_set(bitmap);
  assert(mulib_parsec_run(&set, "a", 1, NULL, &result) == 1); mulib_parsec_result_free(&result);
  look = mulib_parsec_lookahead(&any, 1);
  assert(mulib_parsec_run(&look, "a", 1, NULL, &result) == 1 && !result.consumed);
  mulib_parsec_result_free(&result);
  assert(mulib_parsec_run(&empty, NULL, 0, NULL, &result) == 1); mulib_parsec_result_free(&result);
  slot = &leaf; ref = mulib_parsec_ref(&slot);
  assert(mulib_parsec_run(&ref, "a", 1, NULL, &result) == 1); mulib_parsec_result_free(&result);
  mulib_parsec_result_free(NULL);
  assert(live_allocations == 0);
}
int main(void) {
  json_contracts(); vec_contracts(); parsec_contracts();
  puts("Contract semantics: PASS");
  return 0;
}
