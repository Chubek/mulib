/**
 * @file mulib-edbuf.h
 * @brief Transactional editor buffers: gap, persistent AVL rope and piece table.
 * Define MULIB_IMPL_EDBUF in one C11 translation unit. Offsets/columns are
 * bytes, lines are LF-delimited, and embedded NUL bytes are supported.
 * No encoding, terminal I/O or filesystem policy is imposed.
 */
#ifndef MULIB_EDBUF_H
#define MULIB_EDBUF_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_edbuf mulib_edbuf_t;
typedef struct mulib_edbuf_mark mulib_edbuf_mark_t;
typedef enum mulib_edbuf_kind {
  MULIB_EDBUF_GAP, MULIB_EDBUF_ROPE, MULIB_EDBUF_PIECE_TABLE
} mulib_edbuf_kind_t;
enum mulib_edbuf_result {
  MULIB_EDBUF_OK = 0, MULIB_EDBUF_EINVAL = -1, MULIB_EDBUF_ENOMEM = -2,
  MULIB_EDBUF_ERANGE = -3, MULIB_EDBUF_ENOTFOUND = -4
};
/** @brief Most recent edit; revisions advance on edits, undo and redo.
 * An empty replace is a no-op. A new buffer has revision zero. */
typedef struct mulib_edbuf_change {
  uint64_t before, revision;
  size_t offset, removed, inserted;
} mulib_edbuf_change_t;
/** @brief Copy initial bytes; NULL only allowed for length zero. Default
 * history retains 128 edits. NULL on invalid kind, overflow or allocation. */
mulib_edbuf_t *mulib_edbuf_create(mulib_edbuf_kind_t kind, const void *bytes, size_t length);
/** @brief Free buffer/history. Independently owned marks become detached. */
void mulib_edbuf_destroy(mulib_edbuf_t *buffer);
/** @brief Size in bytes; zero for NULL. */
size_t mulib_edbuf_size(const mulib_edbuf_t *buffer);
/** @brief Current monotonic revision; zero for NULL. */
uint64_t mulib_edbuf_revision(const mulib_edbuf_t *buffer);
/** @brief Borrow latest change until next edit; NULL before first edit. */
const mulib_edbuf_change_t *mulib_edbuf_last_change(const mulib_edbuf_t *buffer);
/** @brief Copy exactly length bytes in bounds. Failure leaves output unchanged. */
int mulib_edbuf_read(const mulib_edbuf_t *, size_t offset, void *output, size_t length);
/** @brief Owned, NUL-terminated copy (may contain earlier NULs); free with free(). */
char *mulib_edbuf_copy(const mulib_edbuf_t *);
/** @brief Atomically replace a byte range, updating marks/history/revision.
 * Failure preserves all observable state. Max size is SIZE_MAX-1 bytes. */
int mulib_edbuf_replace(mulib_edbuf_t *, size_t offset, size_t removed,
                         const void *inserted, size_t length);
/** @brief Insert bytes; same contracts as replace. */
int mulib_edbuf_insert(mulib_edbuf_t *, size_t offset, const void *, size_t length);
/** @brief Erase bytes; same contracts as replace. */
int mulib_edbuf_erase(mulib_edbuf_t *, size_t offset, size_t length);
/** @brief Number of LF-delimited lines, including a final empty line; NULL=0. */
size_t mulib_edbuf_line_count(const mulib_edbuf_t *);
/** @brief Zero-based line start; output unchanged on invalid line. */
int mulib_edbuf_line_start(const mulib_edbuf_t *, size_t line, size_t *offset);
/** @brief Map an offset (including EOF) to zero-based line/byte column. */
int mulib_edbuf_position(const mulib_edbuf_t *, size_t offset, size_t *line, size_t *column);
/** @brief Binary literal KMP search at/after start; empty needle matches start.
 * ENOTFOUND preserves output. */
int mulib_edbuf_find(const mulib_edbuf_t *, const void *needle, size_t length,
                     size_t start, size_t *offset);
/** @brief Clear undo/redo and set maximum edit entries; zero disables history. */
void mulib_edbuf_history_limit(mulib_edbuf_t *, size_t entries);
/** @brief Atomically undo one edit; ENOTFOUND if empty. */
int mulib_edbuf_undo(mulib_edbuf_t *);
/** @brief Atomically redo one edit; a new edit discards redo history. */
int mulib_edbuf_redo(mulib_edbuf_t *);
/** @brief Create an owned mark. Right gravity moves after insertions at mark.
 * Erased interiors collapse; undo does not restore their original positions. */
mulib_edbuf_mark_t *mulib_edbuf_mark_create(mulib_edbuf_t *, size_t offset, int right_gravity);
/** @brief Free/detach a mark, also safe after its buffer was destroyed. */
void mulib_edbuf_mark_destroy(mulib_edbuf_mark_t *);
/** @brief Mark offset, or SIZE_MAX for NULL/detached mark. */
size_t mulib_edbuf_mark_offset(const mulib_edbuf_mark_t *);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_EDBUF) && !defined(MULIB_EDBUF_IMPLEMENTED)
#define MULIB_EDBUF_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_edbuf_node_ {
  struct mulib_edbuf_node_ *left, *right;
  size_t size, lines, refs;
  unsigned height;
  /* Leaves store bytes immediately following the node. */
} mulib_edbuf_node_;
typedef struct mulib_edbuf_block_ {
  struct mulib_edbuf_block_ *next;
} mulib_edbuf_block_;
typedef struct mulib_edbuf_piece_ {
  mulib_edbuf_block_ *block;
  size_t offset, size;
} mulib_edbuf_piece_;
typedef struct mulib_edbuf_edit_ {
  struct mulib_edbuf_edit_ *next;
  size_t offset, removed, inserted;
} mulib_edbuf_edit_;
struct mulib_edbuf_mark {
  mulib_edbuf_t *owner;
  struct mulib_edbuf_mark *next, *previous;
  size_t offset;
  int right;
};
struct mulib_edbuf {
  mulib_edbuf_kind_t kind;
  size_t size, lines, limit, undo_count;
  uint64_t revision;
  mulib_edbuf_change_t change;
  unsigned char *gap;
  size_t capacity, low, high;
  mulib_edbuf_node_ *root;
  mulib_edbuf_block_ *blocks;
  mulib_edbuf_piece_ *pieces;
  size_t piece_count;
  mulib_edbuf_edit_ *undo, *redo;
  mulib_edbuf_mark_t *marks;
};
static size_t mulib_edbuf_lfs_(const unsigned char *s, size_t n) {
  size_t count = 0, i;
  for (i = 0; i < n; ++i) count += s[i] == '\n';
  return count;
}
static unsigned mulib_edbuf_height_(const mulib_edbuf_node_ *n) { return n ? n->height : 0; }
static mulib_edbuf_node_ *mulib_edbuf_hold_(mulib_edbuf_node_ *n) {
  if (n) ++n->refs;
  return n;
}
static void mulib_edbuf_drop_(mulib_edbuf_node_ *n) {
  if (n && !--n->refs) {
    mulib_edbuf_drop_(n->left); mulib_edbuf_drop_(n->right); free(n);
  }
}
static mulib_edbuf_node_ *mulib_edbuf_leaf_(const unsigned char *s, size_t n) {
  mulib_edbuf_node_ *r = (mulib_edbuf_node_ *)calloc(1, sizeof *r + n);
  if (!r) return NULL;
  r->size = n; r->lines = mulib_edbuf_lfs_(s, n); r->height = 1; r->refs = 1;
  memcpy(r + 1, s, n); return r;
}
static mulib_edbuf_node_ *mulib_edbuf_pair_(mulib_edbuf_node_ *a, mulib_edbuf_node_ *b) {
  mulib_edbuf_node_ *r;
  if (!a || !b) return mulib_edbuf_hold_(a ? a : b);
  r = (mulib_edbuf_node_ *)calloc(1, sizeof *r);
  if (!r) return NULL;
  r->left = mulib_edbuf_hold_(a); r->right = mulib_edbuf_hold_(b);
  r->size = a->size + b->size; r->lines = a->lines + b->lines; r->refs = 1;
  r->height = 1 + (a->height > b->height ? a->height : b->height); return r;
}
/* Persistent AVL rotations. Inputs are borrowed; output is one owned ref. */
static mulib_edbuf_node_ *mulib_edbuf_balance_(mulib_edbuf_node_ *a, mulib_edbuf_node_ *b) {
  mulib_edbuf_node_ *x, *y, *r;
  unsigned ha = mulib_edbuf_height_(a), hb = mulib_edbuf_height_(b);
  if (ha > hb + 1) {
    if (mulib_edbuf_height_(a->left) >= mulib_edbuf_height_(a->right)) {
      x = mulib_edbuf_pair_(a->right, b);
      if (!x) return NULL;
      r = mulib_edbuf_pair_(a->left, x); mulib_edbuf_drop_(x); return r;
    }
    x = mulib_edbuf_pair_(a->left, a->right->left);
    if (!x) return NULL;
    y = mulib_edbuf_pair_(a->right->right, b);
  } else if (hb > ha + 1) {
    if (mulib_edbuf_height_(b->right) >= mulib_edbuf_height_(b->left)) {
      x = mulib_edbuf_pair_(a, b->left);
      if (!x) return NULL;
      r = mulib_edbuf_pair_(x, b->right); mulib_edbuf_drop_(x); return r;
    }
    x = mulib_edbuf_pair_(a, b->left->left);
    if (!x) return NULL;
    y = mulib_edbuf_pair_(b->left->right, b->right);
  } else return mulib_edbuf_pair_(a, b);
  if (!y) { mulib_edbuf_drop_(x); return NULL; }
  r = mulib_edbuf_pair_(x, y); mulib_edbuf_drop_(x); mulib_edbuf_drop_(y); return r;
}
static mulib_edbuf_node_ *mulib_edbuf_join_(mulib_edbuf_node_ *a, mulib_edbuf_node_ *b) {
  mulib_edbuf_node_ *x, *r;
  if (!a || !b) return mulib_edbuf_hold_(a ? a : b);
  if (a->height > b->height + 1) {
    x = mulib_edbuf_join_(a->right, b);
    if (!x) return NULL;
    r = mulib_edbuf_balance_(a->left, x);
  } else if (b->height > a->height + 1) {
    x = mulib_edbuf_join_(a, b->left);
    if (!x) return NULL;
    r = mulib_edbuf_balance_(x, b->right);
  } else return mulib_edbuf_pair_(a, b);
  mulib_edbuf_drop_(x); return r;
}
static mulib_edbuf_node_ *mulib_edbuf_tree_(const unsigned char *s, size_t n) {
  mulib_edbuf_node_ *a, *b, *r;
  size_t half;
  if (!n) return NULL;
  if (n <= 1024) return mulib_edbuf_leaf_(s, n);
  half = n / 2;
  a = mulib_edbuf_tree_(s, half);
  if (!a) return NULL;
  b = mulib_edbuf_tree_(s + half, n - half);
  r = b ? mulib_edbuf_pair_(a, b) : NULL;
  mulib_edbuf_drop_(a); mulib_edbuf_drop_(b); return r;
}
static int mulib_edbuf_split_(mulib_edbuf_node_ *n, size_t at,
                               mulib_edbuf_node_ **left, mulib_edbuf_node_ **right) {
  mulib_edbuf_node_ *a = NULL, *b = NULL, *r;
  *left = *right = NULL;
  if (!n) return 1;
  if (!at) { *right = mulib_edbuf_hold_(n); return 1; }
  if (at == n->size) { *left = mulib_edbuf_hold_(n); return 1; }
  if (!n->left) {
    a = mulib_edbuf_leaf_((const unsigned char *)(n + 1), at);
    b = mulib_edbuf_leaf_((const unsigned char *)(n + 1) + at, n->size - at);
    if (!a || !b) { mulib_edbuf_drop_(a); mulib_edbuf_drop_(b); return 0; }
    *left = a; *right = b; return 1;
  }
  if (at < n->left->size) {
    if (!mulib_edbuf_split_(n->left, at, &a, &b)) return 0;
    r = mulib_edbuf_join_(b, n->right); mulib_edbuf_drop_(b);
    if (!r) { mulib_edbuf_drop_(a); return 0; }
    *left = a; *right = r;
  } else {
    if (!mulib_edbuf_split_(n->right, at - n->left->size, &a, &b)) return 0;
    r = mulib_edbuf_join_(n->left, a); mulib_edbuf_drop_(a);
    if (!r) { mulib_edbuf_drop_(b); return 0; }
    *left = r; *right = b;
  }
  return 1;
}
static void mulib_edbuf_tree_read_(const mulib_edbuf_node_ *r, size_t at, unsigned char *out, size_t n) {
  size_t first;
  if (!n) return;
  if (!r->left) { memcpy(out, (const unsigned char *)(r + 1) + at, n); return; }
  first = at < r->left->size ? r->left->size - at : 0;
  if (first > n) first = n;
  if (first) mulib_edbuf_tree_read_(r->left, at, out, first);
  if (first < n) mulib_edbuf_tree_read_(r->right, at + first - r->left->size, out + first, n - first);
}
int mulib_edbuf_read(const mulib_edbuf_t *b, size_t at, void *output, size_t n) {
  unsigned char *out = (unsigned char *)output;
  size_t k, first;
  if (!b || (!out && n)) return MULIB_EDBUF_EINVAL;
  if (at > b->size || n > b->size - at) return MULIB_EDBUF_ERANGE;
  if (!n) return 0;
  if (b->kind == MULIB_EDBUF_ROPE) mulib_edbuf_tree_read_(b->root, at, out, n);
  else if (b->kind == MULIB_EDBUF_GAP) {
    first = at < b->low ? b->low - at : 0;
    if (first > n) first = n;
    if (first) memcpy(out, b->gap + at, first);
    if (first < n) memcpy(out + first, b->gap + at + first + b->high - b->low, n - first);
  } else for (k = 0; n && k < b->piece_count; ++k) {
    const mulib_edbuf_piece_ *p = &b->pieces[k];
    if (at >= p->size) { at -= p->size; continue; }
    first = p->size - at; if (first > n) first = n;
    memcpy(out, (const unsigned char *)(p->block + 1) + p->offset + at, first);
    out += first; n -= first; at = 0;
  }
  return 0;
}
size_t mulib_edbuf_size(const mulib_edbuf_t *b) { return b ? b->size : 0; }
uint64_t mulib_edbuf_revision(const mulib_edbuf_t *b) { return b ? b->revision : 0; }
const mulib_edbuf_change_t *mulib_edbuf_last_change(const mulib_edbuf_t *b) {
  return b && b->revision ? &b->change : NULL;
}
char *mulib_edbuf_copy(const mulib_edbuf_t *b) {
  char *out;
  if (!b) return NULL;
  out = (char *)malloc(b->size + 1);
  if (out) { (void)mulib_edbuf_read(b, 0, out, b->size); out[b->size] = 0; }
  return out;
}
static int mulib_edbuf_rope_edit_(mulib_edbuf_t *b, size_t at, size_t del, const void *s, size_t n) {
  mulib_edbuf_node_ *a = NULL, *rest = NULL, *gone = NULL, *c = NULL;
  mulib_edbuf_node_ *insert = NULL, *ac = NULL, *root = NULL;
  int ok = 0;
  if (!mulib_edbuf_split_(b->root, at, &a, &rest)
      || !mulib_edbuf_split_(rest, del, &gone, &c)) goto done;
  insert = mulib_edbuf_tree_((const unsigned char *)s, n);
  if (n && !insert) goto done;
  ac = mulib_edbuf_join_(a, insert);
  if ((a || insert) && !ac) goto done;
  root = mulib_edbuf_join_(ac, c);
  if ((ac || c) && !root) goto done;
  mulib_edbuf_drop_(b->root); b->root = root; root = NULL; ok = 1;
done:
  mulib_edbuf_drop_(a); mulib_edbuf_drop_(rest); mulib_edbuf_drop_(gone);
  mulib_edbuf_drop_(c); mulib_edbuf_drop_(insert); mulib_edbuf_drop_(ac); mulib_edbuf_drop_(root);
  return ok;
}
static void mulib_edbuf_piece_add_(mulib_edbuf_piece_ *out, size_t *count,
                                   mulib_edbuf_block_ *block, size_t at, size_t n) {
  mulib_edbuf_piece_ *p;
  if (!n) return;
  if (*count) {
    p = &out[*count - 1];
    if (p->block == block && p->offset + p->size == at) { p->size += n; return; }
  }
  p = &out[(*count)++]; p->block = block; p->offset = at; p->size = n;
}
static int mulib_edbuf_piece_edit_(mulib_edbuf_t *b, size_t at, size_t del, const void *s, size_t n) {
  mulib_edbuf_piece_ *out;
  mulib_edbuf_block_ *block = NULL;
  size_t i, count = 0, pos = 0, end = at + del;
  if (b->piece_count > SIZE_MAX / sizeof *out - 3 || n > SIZE_MAX - sizeof *block) return 0;
  out = (mulib_edbuf_piece_ *)malloc((b->piece_count + 3) * sizeof *out);
  if (!out) return 0;
  if (n) {
    block = (mulib_edbuf_block_ *)malloc(sizeof *block + n);
    if (!block) { free(out); return 0; }
    memcpy(block + 1, s, n);
  }
  for (i = 0; i < b->piece_count; ++i) {
    mulib_edbuf_piece_ p = b->pieces[i];
    size_t take = at > pos ? at - pos : 0;
    if (take > p.size) take = p.size;
    mulib_edbuf_piece_add_(out, &count, p.block, p.offset, take); pos += p.size;
  }
  mulib_edbuf_piece_add_(out, &count, block, 0, n); pos = 0;
  for (i = 0; i < b->piece_count; ++i) {
    mulib_edbuf_piece_ p = b->pieces[i];
    size_t skip = end > pos ? end - pos : 0;
    if (skip > p.size) skip = p.size;
    mulib_edbuf_piece_add_(out, &count, p.block, p.offset + skip, p.size - skip); pos += p.size;
  }
  if (block) { block->next = b->blocks; b->blocks = block; }
  free(b->pieces); b->pieces = out; b->piece_count = count; return 1;
}
static int mulib_edbuf_gap_edit_(mulib_edbuf_t *b, size_t at, size_t del, const void *s, size_t n) {
  size_t size = b->size - del + n, tail = b->size - at - del;
  if (size > b->capacity) {
    size_t cap = b->capacity ? b->capacity : 128;
    unsigned char *p;
    while (cap < size) { if (cap > SIZE_MAX / 2) { cap = size; break; } cap *= 2; }
    p = (unsigned char *)malloc(cap);
    if (!p) return 0;
    (void)mulib_edbuf_read(b, 0, p, at);
    if (n) memcpy(p + at, s, n);
    (void)mulib_edbuf_read(b, at + del, p + cap - tail, tail);
    free(b->gap); b->gap = p; b->capacity = cap; b->low = at + n; b->high = cap - tail;
  } else {
    if (at < b->low) {
      size_t move = b->low - at;
      memmove(b->gap + b->high - move, b->gap + at, move); b->low -= move; b->high -= move;
    } else if (at > b->low) {
      size_t move = at - b->low;
      memmove(b->gap + b->low, b->gap + b->high, move); b->low += move; b->high += move;
    }
    b->high += del;
    if (n) memcpy(b->gap + b->low, s, n);
    b->low += n;
  }
  return 1;
}
static size_t mulib_edbuf_range_lfs_(const mulib_edbuf_t *b, size_t at, size_t n) {
  unsigned char bytes[1024];
  size_t count = 0;
  while (n) {
    size_t take = n < sizeof bytes ? n : sizeof bytes;
    (void)mulib_edbuf_read(b, at, bytes, take); count += mulib_edbuf_lfs_(bytes, take);
    at += take; n -= take;
  }
  return count;
}
static void mulib_edbuf_edits_free_(mulib_edbuf_edit_ *e) {
  while (e) { mulib_edbuf_edit_ *next = e->next; free(e); e = next; }
}
static int mulib_edbuf_mutate_(mulib_edbuf_t *b, size_t at, size_t del, const void *s, size_t n, int history) {
  mulib_edbuf_edit_ *edit = NULL;
  mulib_edbuf_mark_t *mark;
  size_t removed_lines, added_lines;
  int ok;
  if (!b || (!s && n)) return MULIB_EDBUF_EINVAL;
  if (at > b->size || del > b->size - at || n > SIZE_MAX - 1 - (b->size - del)) return MULIB_EDBUF_ERANGE;
  if (!del && !n) return 0;
  if (b->revision == UINT64_MAX) return MULIB_EDBUF_ERANGE;
  removed_lines = mulib_edbuf_range_lfs_(b, at, del);
  added_lines = mulib_edbuf_lfs_((const unsigned char *)s, n);
  if (history && b->limit) {
    if (del > SIZE_MAX - sizeof *edit || n > SIZE_MAX - sizeof *edit - del) return MULIB_EDBUF_ERANGE;
    edit = (mulib_edbuf_edit_ *)malloc(sizeof *edit + del + n);
    if (!edit) return MULIB_EDBUF_ENOMEM;
    edit->offset = at; edit->removed = del; edit->inserted = n;
    (void)mulib_edbuf_read(b, at, edit + 1, del);
    if (n) memcpy((unsigned char *)(edit + 1) + del, s, n);
  }
  if (b->kind == MULIB_EDBUF_GAP) ok = mulib_edbuf_gap_edit_(b, at, del, s, n);
  else if (b->kind == MULIB_EDBUF_ROPE) ok = mulib_edbuf_rope_edit_(b, at, del, s, n);
  else ok = mulib_edbuf_piece_edit_(b, at, del, s, n);
  if (!ok) { free(edit); return MULIB_EDBUF_ENOMEM; }
  b->change.before = b->revision; b->change.revision = ++b->revision;
  b->change.offset = at; b->change.removed = del; b->change.inserted = n;
  b->size = b->size - del + n; b->lines = b->lines - removed_lines + added_lines;
  for (mark = b->marks; mark; mark = mark->next) {
    if (mark->offset < at) continue;
    if (mark->offset > at + del) mark->offset = at + n + (mark->offset - at - del);
    else if (del && mark->offset == at + del) mark->offset = at + n;
    else mark->offset = at + (mark->right ? n : 0);
  }
  if (history) {
    mulib_edbuf_edits_free_(b->redo); b->redo = NULL;
    if (edit) {
      edit->next = b->undo; b->undo = edit; ++b->undo_count;
      if (b->undo_count > b->limit) {
        mulib_edbuf_edit_ **p = &b->undo;
        size_t keep = b->limit;
        while (keep--) p = &(*p)->next;
        mulib_edbuf_edits_free_(*p); *p = NULL; b->undo_count = b->limit;
      }
    }
  }
  return 0;
}
int mulib_edbuf_replace(mulib_edbuf_t *b, size_t at, size_t del, const void *s, size_t n) {
  return mulib_edbuf_mutate_(b, at, del, s, n, 1);
}
int mulib_edbuf_insert(mulib_edbuf_t *b, size_t at, const void *s, size_t n) { return mulib_edbuf_replace(b, at, 0, s, n); }
int mulib_edbuf_erase(mulib_edbuf_t *b, size_t at, size_t n) { return mulib_edbuf_replace(b, at, n, NULL, 0); }
void mulib_edbuf_history_limit(mulib_edbuf_t *b, size_t limit) {
  if (!b) return;
  mulib_edbuf_edits_free_(b->undo); mulib_edbuf_edits_free_(b->redo);
  b->undo = b->redo = NULL; b->undo_count = 0; b->limit = limit;
}
int mulib_edbuf_undo(mulib_edbuf_t *b) {
  mulib_edbuf_edit_ *e;
  int r;
  if (!b) return MULIB_EDBUF_EINVAL;
  if (!(e = b->undo)) return MULIB_EDBUF_ENOTFOUND;
  r = mulib_edbuf_mutate_(b, e->offset, e->inserted, e + 1, e->removed, 0);
  if (!r) { b->undo = e->next; --b->undo_count; e->next = b->redo; b->redo = e; }
  return r;
}
int mulib_edbuf_redo(mulib_edbuf_t *b) {
  mulib_edbuf_edit_ *e;
  int r;
  if (!b) return MULIB_EDBUF_EINVAL;
  if (!(e = b->redo)) return MULIB_EDBUF_ENOTFOUND;
  r = mulib_edbuf_mutate_(b, e->offset, e->removed, (unsigned char *)(e + 1) + e->removed, e->inserted, 0);
  if (!r) { b->redo = e->next; e->next = b->undo; b->undo = e; ++b->undo_count; }
  return r;
}
mulib_edbuf_t *mulib_edbuf_create(mulib_edbuf_kind_t kind, const void *s, size_t n) {
  mulib_edbuf_t *b;
  if (kind < MULIB_EDBUF_GAP || kind > MULIB_EDBUF_PIECE_TABLE || (!s && n) || n == SIZE_MAX) return NULL;
  b = (mulib_edbuf_t *)calloc(1, sizeof *b);
  if (!b) return NULL;
  b->kind = kind; b->lines = 1;
  if (mulib_edbuf_mutate_(b, 0, 0, s, n, 0)) { mulib_edbuf_destroy(b); return NULL; }
  b->revision = 0; memset(&b->change, 0, sizeof b->change); b->limit = 128; return b;
}
void mulib_edbuf_destroy(mulib_edbuf_t *b) {
  mulib_edbuf_mark_t *m;
  if (!b) return;
  for (m = b->marks; m; m = m->next) m->owner = NULL;
  mulib_edbuf_history_limit(b, 0); free(b->gap); mulib_edbuf_drop_(b->root); free(b->pieces);
  while (b->blocks) { mulib_edbuf_block_ *next = b->blocks->next; free(b->blocks); b->blocks = next; }
  free(b);
}
size_t mulib_edbuf_line_count(const mulib_edbuf_t *b) { return b ? b->lines : 0; }
static size_t mulib_edbuf_nth_lf_(const mulib_edbuf_node_ *r, size_t nth) {
  size_t i;
  if (r->left) {
    if (nth < r->left->lines) return mulib_edbuf_nth_lf_(r->left, nth);
    return r->left->size + mulib_edbuf_nth_lf_(r->right, nth - r->left->lines);
  }
  for (i = 0; i < r->size; ++i) if (((const unsigned char *)(r + 1))[i] == '\n' && !nth--) return i;
  return r->size;
}
int mulib_edbuf_line_start(const mulib_edbuf_t *b, size_t line, size_t *out) {
  unsigned char data[1024];
  size_t at = 0, i;
  if (!b || !out) return MULIB_EDBUF_EINVAL;
  if (line >= b->lines) return MULIB_EDBUF_ERANGE;
  if (!line) { *out = 0; return 0; }
  if (b->kind == MULIB_EDBUF_ROPE) { *out = mulib_edbuf_nth_lf_(b->root, line - 1) + 1; return 0; }
  while (at < b->size) {
    size_t n = b->size - at; if (n > sizeof data) n = sizeof data;
    (void)mulib_edbuf_read(b, at, data, n);
    for (i = 0; i < n; ++i) if (data[i] == '\n' && !--line) { *out = at + i + 1; return 0; }
    at += n;
  }
  return MULIB_EDBUF_ERANGE;
}
int mulib_edbuf_position(const mulib_edbuf_t *b, size_t at, size_t *line, size_t *column) {
  size_t l, start;
  if (!b || !line || !column) return MULIB_EDBUF_EINVAL;
  if (at > b->size) return MULIB_EDBUF_ERANGE;
  l = mulib_edbuf_range_lfs_(b, 0, at);
  if (mulib_edbuf_line_start(b, l, &start)) return MULIB_EDBUF_ERANGE;
  *line = l; *column = at - start; return 0;
}
int mulib_edbuf_find(const mulib_edbuf_t *b, const void *needle, size_t n, size_t start, size_t *out) {
  const unsigned char *s = (const unsigned char *)needle;
  unsigned char bytes[1024];
  size_t *prefix, i, j = 0, at = start;
  if (!b || !out || (!s && n)) return MULIB_EDBUF_EINVAL;
  if (start > b->size || n > SIZE_MAX / sizeof *prefix) return MULIB_EDBUF_ERANGE;
  if (!n) { *out = start; return 0; }
  if (n > b->size - start) return MULIB_EDBUF_ENOTFOUND;
  prefix = (size_t *)malloc(n * sizeof *prefix);
  if (!prefix) return MULIB_EDBUF_ENOMEM;
  prefix[0] = 0;
  for (i = 1; i < n; ++i) {
    while (j && s[i] != s[j]) j = prefix[j - 1];
    if (s[i] == s[j]) ++j;
    prefix[i] = j;
  }
  j = 0;
  while (at < b->size) {
    size_t take = b->size - at; if (take > sizeof bytes) take = sizeof bytes;
    (void)mulib_edbuf_read(b, at, bytes, take);
    for (i = 0; i < take; ++i) {
      while (j && bytes[i] != s[j]) j = prefix[j - 1];
      if (bytes[i] == s[j]) ++j;
      if (j == n) { *out = at + i + 1 - n; free(prefix); return 0; }
    }
    at += take;
  }
  free(prefix); return MULIB_EDBUF_ENOTFOUND;
}
mulib_edbuf_mark_t *mulib_edbuf_mark_create(mulib_edbuf_t *b, size_t at, int right) {
  mulib_edbuf_mark_t *m;
  if (!b || at > b->size) return NULL;
  m = (mulib_edbuf_mark_t *)calloc(1, sizeof *m);
  if (!m) return NULL;
  m->owner = b; m->offset = at; m->right = !!right; m->next = b->marks;
  if (m->next) m->next->previous = m;
  b->marks = m; return m;
}
void mulib_edbuf_mark_destroy(mulib_edbuf_mark_t *m) {
  if (!m) return;
  if (m->owner) {
    if (m->previous) m->previous->next = m->next; else m->owner->marks = m->next;
    if (m->next) m->next->previous = m->previous;
  }
  free(m);
}
size_t mulib_edbuf_mark_offset(const mulib_edbuf_mark_t *m) { return m && m->owner ? m->offset : SIZE_MAX; }
#ifdef __cplusplus
}
#endif
#endif
