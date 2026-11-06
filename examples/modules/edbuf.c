#define MULIB_IMPL_EDBUF
#include "mulib-edbuf.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
  int kind;
  for (kind = MULIB_EDBUF_GAP; kind <= MULIB_EDBUF_PIECE_TABLE; ++kind) {
    mulib_edbuf_t *b = mulib_edbuf_create((mulib_edbuf_kind_t)kind,"hello\n",6);
    mulib_edbuf_mark_t *cursor = mulib_edbuf_mark_create(b,5,1);
    size_t line, column;
    char *text;
    assert(b && cursor && !mulib_edbuf_insert(b,5,", editor",8));
    assert(mulib_edbuf_mark_offset(cursor) == 13);
    assert(!mulib_edbuf_position(b,13,&line,&column) && line == 0 && column == 13);
    assert(!mulib_edbuf_undo(b) && !mulib_edbuf_redo(b));
    text = mulib_edbuf_copy(b); assert(text && !strcmp(text,"hello, editor\n"));
    fputs(text,stdout); free(text); mulib_edbuf_mark_destroy(cursor); mulib_edbuf_destroy(b);
  }
  return 0;
}
