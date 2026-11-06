#define MULIB_IMPL_EDBUF
#include "mulib-edbuf.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rng = 12345;
static uint32_t random_value(void) { rng = rng * 1664525u + 1013904223u; return rng; }
static void validate_tree(const mulib_edbuf_node_ *n) {
  if (!n) return;
  assert(n->refs);
  if (!n->left) { assert(!n->right && n->height == 1 && n->size <= 1024); return; }
  assert(n->right && n->size == n->left->size + n->right->size);
  assert(n->lines == n->left->lines + n->right->lines);
  assert(n->left->height <= n->right->height + 1 && n->right->height <= n->left->height + 1);
  assert(n->height == 1 + (n->left->height > n->right->height ? n->left->height : n->right->height));
  validate_tree(n->left); validate_tree(n->right);
}
static void random_edits(mulib_edbuf_kind_t kind) {
  unsigned char model[65536], inserted[64], actual[65536];
  size_t size = 10000, step, i;
  mulib_edbuf_t *b;
  memset(model, 'x', size); model[2048] = '\n';
  b = mulib_edbuf_create(kind, model, size); assert(b);
  mulib_edbuf_history_limit(b, 0);
  for (step = 0; step < 3000; ++step) {
    size_t at = random_value() % (size + 1), del = random_value() % 43, n = random_value() % 41;
    size_t lines = 1, line, column, start;
    if (del > size - at) del = size - at;
    for (i = 0; i < n; ++i) inserted[i] = (unsigned char)(random_value() % 128);
    assert(!mulib_edbuf_replace(b, at, del, inserted, n));
    memmove(model + at + n, model + at + del, size - at - del);
    memcpy(model + at, inserted, n); size = size - del + n;
    assert(mulib_edbuf_size(b) == size && !mulib_edbuf_read(b, 0, actual, size));
    assert(!memcmp(model, actual, size));
    for (i = 0; i < size; ++i) lines += model[i] == '\n';
    assert(mulib_edbuf_line_count(b) == lines);
    if (step % 17 == 0) {
      assert(!mulib_edbuf_position(b, at, &line, &column));
      assert(!mulib_edbuf_line_start(b, line, &start) && start + column == at);
      assert(!mulib_edbuf_line_start(b, lines - 1, &start));
      assert(start <= size && (!start || model[start-1] == '\n'));
      validate_tree(b->root);
    }
  }
  mulib_edbuf_destroy(b);
}
static void history_marks(mulib_edbuf_kind_t kind) {
  mulib_edbuf_t *b = mulib_edbuf_create(kind, "one\ntwo\n", 8);
  mulib_edbuf_mark_t *left, *right, *end;
  char *copy;
  size_t at = 88, line, column;
  uint64_t revision;
  assert(b && mulib_edbuf_line_count(b) == 3);
  left = mulib_edbuf_mark_create(b, 4, 0); right = mulib_edbuf_mark_create(b, 4, 1); end = mulib_edbuf_mark_create(b, 8, 0);
  assert(left && right && end);
  assert(!mulib_edbuf_insert(b, 4, "new ", 4));
  assert(mulib_edbuf_mark_offset(left) == 4 && mulib_edbuf_mark_offset(right) == 8 && mulib_edbuf_mark_offset(end) == 12);
  assert(!mulib_edbuf_erase(b, 0, 4));
  assert(mulib_edbuf_mark_offset(right) == 4);
  assert(!mulib_edbuf_undo(b) && !mulib_edbuf_undo(b));
  copy = mulib_edbuf_copy(b); assert(copy && !strcmp(copy, "one\ntwo\n")); free(copy);
  assert(!mulib_edbuf_redo(b) && !mulib_edbuf_redo(b));
  copy = mulib_edbuf_copy(b); assert(copy && !strcmp(copy, "new two\n")); free(copy);
  assert(!mulib_edbuf_find(b, "two", 3, 0, &at) && at == 4);
  assert(mulib_edbuf_find(b, "absent", 6, 0, &at) == MULIB_EDBUF_ENOTFOUND && at == 4);
  assert(!mulib_edbuf_position(b, 8, &line, &column) && line == 1 && column == 0);
  assert(!mulib_edbuf_undo(b));
  assert(!mulib_edbuf_insert(b, 0, "!", 1));
  assert(mulib_edbuf_redo(b) == MULIB_EDBUF_ENOTFOUND);
  revision = mulib_edbuf_revision(b);
  assert(!mulib_edbuf_replace(b, 0, 0, NULL, 0) && mulib_edbuf_revision(b) == revision);
  assert(mulib_edbuf_erase(b, SIZE_MAX, 1) == MULIB_EDBUF_ERANGE);
  assert(mulib_edbuf_insert(b, 0, "x", SIZE_MAX) == MULIB_EDBUF_ERANGE);
  assert(mulib_edbuf_revision(b) == revision);
  mulib_edbuf_history_limit(b, 1);
  assert(!mulib_edbuf_insert(b, 0, "a", 1) && !mulib_edbuf_insert(b, 0, "b", 1));
  assert(!mulib_edbuf_undo(b) && mulib_edbuf_undo(b) == MULIB_EDBUF_ENOTFOUND);
  mulib_edbuf_mark_destroy(left); mulib_edbuf_mark_destroy(right);
  mulib_edbuf_destroy(b); assert(mulib_edbuf_mark_offset(end) == SIZE_MAX); mulib_edbuf_mark_destroy(end);
}
int main(void) {
  int kind;
  assert(!mulib_edbuf_create((mulib_edbuf_kind_t)99, NULL, 0));
  assert(!mulib_edbuf_create(MULIB_EDBUF_GAP, NULL, 1));
  for (kind = 0; kind < 3; ++kind) {
    mulib_edbuf_t *empty = mulib_edbuf_create((mulib_edbuf_kind_t)kind, NULL, 0);
    size_t at = 9;
    char *s;
    assert(empty && mulib_edbuf_line_count(empty) == 1);
    assert(!mulib_edbuf_find(empty, NULL, 0, 0, &at) && at == 0);
    s = mulib_edbuf_copy(empty); assert(s && !*s); free(s); mulib_edbuf_destroy(empty);
    random_edits((mulib_edbuf_kind_t)kind); history_marks((mulib_edbuf_kind_t)kind);
  }
  puts("edbuf: all backends, randomized edits, lines, history and marks passed"); return 0;
}
