/**
 * @file mulib-imtui-edit.h
 * @brief Caller-buffered single-line UTF-8 editor for the immediate-mode TUI.
 *
 * Define MULIB_IMPL_IMTUI_EDIT in one translation unit. Editing is by Unicode
 * scalar, not by grapheme cluster. No allocation or system clipboard access.
 */
#ifndef MULIB_IMTUI_EDIT_H
#define MULIB_IMTUI_EDIT_H
#include "mulib-imtui-draw.h"
#include "mulib-utf8.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mulib_imtui_edit_state {
  /** Byte offsets in the current buffer, including its end. */
  size_t cursor, anchor, first;
  /* Internal streaming paste state. */
  unsigned paste, paste_full, paste_cr, pending_len, pending_need;
  unsigned char pending[4];
} mulib_imtui_edit_state_t;

enum mulib_imtui_edit_result {
  MULIB_IMTUI_EDIT_CHANGED = 1u,
  MULIB_IMTUI_EDIT_SUBMITTED = 2u,
  /** An insertion did not fit; an overlong paste keeps only its prefix. */
  MULIB_IMTUI_EDIT_FULL = 4u
};

/** Initialize or reset selection, viewport and pending paste state. */
void mulib_imtui_edit_init(mulib_imtui_edit_state_t *, size_t cursor);

/**
 * Edit and draw the first row of rect. Capacity includes the trailing NUL.
 * The caller provides *length bytes of valid single-line UTF-8, without C0,
 * DEL, C1 or Unicode line/paragraph separators, followed by a NUL, and keeps
 * the state with the same widget ID. Invalid input is unchanged and returns
 * zero. Offsets are clamped to scalar boundaries. The focused caret is
 * underlined and the selection uses inverse video.
 *
 * Left/Right, Home/End and Shift selection are supported. Ctrl+Left/Right
 * group ASCII word characters and non-ASCII scalars; Ctrl+A selects all.
 * Backspace and Delete replace a selection first. Enter reports submission.
 * Paste accepts a maximal prefix of whole scalars, including scalars split
 * across events/frames. Invalid UTF-8 and other terminal controls are dropped;
 * CRLF, CR, LF, tab and Unicode line/paragraph separators become spaces.
 * Disabling or losing focus discards any incomplete paste.
 */
unsigned mulib_imtui_edit(mulib_imtui_t *, mulib_imtui_id_t, mulib_imtui_rect_t,
                         mulib_imtui_edit_state_t *, char *buffer, size_t *length,
                         size_t capacity, mulib_term_style_t, int enabled);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMTUI_EDIT_H */

#if defined(MULIB_IMPL_IMTUI_EDIT) && !defined(MULIB_IMTUI_EDIT_IMPLEMENTED)
#define MULIB_IMTUI_EDIT_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

void mulib_imtui_edit_init(mulib_imtui_edit_state_t *s, size_t cursor) {
  if (!s) return;
  memset(s, 0, sizeof *s);
  s->cursor = s->anchor = cursor;
}

static int mulib_imtui_edit_printable_(uint32_t c) {
  return c >= 32 && !(c >= 127 && c < 160) && c != 0x2028 && c != 0x2029;
}

static size_t mulib_imtui_edit_boundary_(const char *buf, size_t n, size_t p) {
  if (p >= n) return n;
  while (p && ((unsigned char)buf[p] & 0xc0u) == 0x80u) --p;
  return p;
}

static size_t mulib_imtui_edit_prev_(const char *buf, size_t p) {
  return p ? mulib_imtui_edit_boundary_(buf, p, p - 1) : 0;
}

static size_t mulib_imtui_edit_next_(const char *buf, size_t n, size_t p) {
  if (p < n) {
    ++p;
    while (p < n && ((unsigned char)buf[p] & 0xc0u) == 0x80u) ++p;
  }
  return p;
}

static size_t mulib_imtui_edit_min_(size_t a, size_t b) { return a < b ? a : b; }
static size_t mulib_imtui_edit_max_(size_t a, size_t b) { return a > b ? a : b; }

static int mulib_imtui_edit_word_(unsigned char c) {
  return c >= 128 || c == '_' || (c >= '0' && c <= '9')
         || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static size_t mulib_imtui_edit_word_prev_(const char *b, size_t p) {
  size_t q = mulib_imtui_edit_prev_(b, p);
  while (p && !mulib_imtui_edit_word_((unsigned char)b[q])) {
    p = q; q = mulib_imtui_edit_prev_(b, p);
  }
  while (p && mulib_imtui_edit_word_((unsigned char)b[q])) {
    p = q; q = mulib_imtui_edit_prev_(b, p);
  }
  return p;
}

static size_t mulib_imtui_edit_word_next_(const char *b, size_t n, size_t p) {
  while (p < n && mulib_imtui_edit_word_((unsigned char)b[p]))
    p = mulib_imtui_edit_next_(b, n, p);
  while (p < n && !mulib_imtui_edit_word_((unsigned char)b[p]))
    p = mulib_imtui_edit_next_(b, n, p);
  return p;
}

/* Replacement is atomic; a non-fitting scalar never erases a selection. */
static unsigned mulib_imtui_edit_replace_(mulib_imtui_edit_state_t *s, char *b,
                                         size_t *n, size_t cap, size_t lo,
                                         size_t hi, const void *text, size_t count) {
  size_t rest = *n - (hi - lo);
  unsigned result;
  if (count > cap - 1 - rest) return MULIB_IMTUI_EDIT_FULL;
  result = count != hi - lo || (count && memcmp(b + lo, text, count))
           ? MULIB_IMTUI_EDIT_CHANGED : 0;
  if (result) {
    memmove(b + lo + count, b + hi, *n - hi + 1);
    if (count) memcpy(b + lo, text, count);
    *n = rest + count;
  }
  if (s->first > lo)
    s->first = s->first < hi ? lo : s->first - (hi - lo) + count;
  s->cursor = s->anchor = lo + count;
  return result;
}

static unsigned mulib_imtui_edit_insert_(mulib_imtui_edit_state_t *s, char *b,
                                        size_t *n, size_t cap, uint32_t cp) {
  unsigned char encoded[4];
  size_t count;
  if (!mulib_imtui_edit_printable_(cp)) return 0;
  count = mulib_utf8_encode(cp, encoded, sizeof encoded);
  if (!count) return 0;
  return mulib_imtui_edit_replace_(s, b, n, cap,
                                  mulib_imtui_edit_min_(s->cursor, s->anchor),
                                  mulib_imtui_edit_max_(s->cursor, s->anchor),
                                  encoded, count);
}

static void mulib_imtui_edit_paste_reset_(mulib_imtui_edit_state_t *s) {
  s->paste = s->paste_full = s->paste_cr = s->pending_len = s->pending_need = 0;
}

static unsigned mulib_imtui_edit_paste_(mulib_imtui_edit_state_t *s, char *b,
                                       size_t *n, size_t cap,
                                       const mulib_termkey_event_t *e) {
  unsigned result = 0;
  size_t i = 0, length = mulib_imtui_edit_min_(e->length, sizeof e->raw);
  while (i < length && !s->paste_full) {
    unsigned char c = e->raw[i];
    uint32_t cp;
    if (s->pending_len && (c & 0xc0u) != 0x80u) {
      s->pending_len = 0; /* Reprocess this byte as a new leading byte. */
      s->paste_cr = 0;
    }
    if (!s->pending_len)
      s->pending_need = c < 0x80 ? 1u
                        : c >= 0xc2 && c <= 0xdf ? 2u
                        : c >= 0xe0 && c <= 0xef ? 3u
                        : c >= 0xf0 && c <= 0xf4 ? 4u : 0u;
    ++i;
    if (!s->pending_need) { s->paste_cr = 0; continue; }
    s->pending[s->pending_len++] = c;
    if (s->pending_len < s->pending_need) continue;
    s->pending_len = 0;
    if (!mulib_utf8_decode(s->pending, s->pending_need, &cp, NULL)) {
      s->paste_cr = 0;
      continue;
    }
    if (cp == '\n' && s->paste_cr) { s->paste_cr = 0; continue; }
    s->paste_cr = cp == '\r';
    if (cp == '\r' || cp == '\n' || cp == '\t' || cp == 0x2028 || cp == 0x2029)
      cp = ' ';
    result |= mulib_imtui_edit_insert_(s, b, n, cap, cp);
    if (result & MULIB_IMTUI_EDIT_FULL) s->paste_full = 1;
  }
  return result;
}

static unsigned mulib_imtui_edit_key_(mulib_imtui_edit_state_t *s, char *b,
                                      size_t *n, size_t cap,
                                      const mulib_termkey_event_t *e) {
  size_t pos = s->cursor, lo = mulib_imtui_edit_min_(pos, s->anchor);
  size_t hi = mulib_imtui_edit_max_(pos, s->anchor);
  unsigned shift = e->modifiers & MULIB_TERMKEY_SHIFT;
  unsigned ctrl = e->modifiers & MULIB_TERMKEY_CTRL;
  if (e->modifiers & (MULIB_TERMKEY_ALT | MULIB_TERMKEY_SUPER)) return 0;
  switch (e->key) {
    case MULIB_TERMKEY_LEFT:
      if (!shift && lo != hi) pos = lo;
      else pos = ctrl ? mulib_imtui_edit_word_prev_(b, pos)
                      : mulib_imtui_edit_prev_(b, pos);
      break;
    case MULIB_TERMKEY_RIGHT:
      if (!shift && lo != hi) pos = hi;
      else pos = ctrl ? mulib_imtui_edit_word_next_(b, *n, pos)
                      : mulib_imtui_edit_next_(b, *n, pos);
      break;
    case MULIB_TERMKEY_HOME: pos = 0; break;
    case MULIB_TERMKEY_END: pos = *n; break;
    case MULIB_TERMKEY_BACKSPACE:
      if (lo == hi)
        lo = ctrl ? mulib_imtui_edit_word_prev_(b, lo) : mulib_imtui_edit_prev_(b, lo);
      return mulib_imtui_edit_replace_(s, b, n, cap, lo, hi, NULL, 0);
    case MULIB_TERMKEY_DELETE:
      if (lo == hi)
        hi = ctrl ? mulib_imtui_edit_word_next_(b, *n, hi) : mulib_imtui_edit_next_(b, *n, hi);
      return mulib_imtui_edit_replace_(s, b, n, cap, lo, hi, NULL, 0);
    case MULIB_TERMKEY_ENTER: return MULIB_IMTUI_EDIT_SUBMITTED;
    case MULIB_TERMKEY_CHAR:
      if (!ctrl) return mulib_imtui_edit_insert_(s, b, n, cap, e->codepoint);
      if (e->codepoint == 'a' || e->codepoint == 'A') {
        s->anchor = 0; s->cursor = *n;
      }
      return 0;
    default: return 0;
  }
  s->cursor = pos;
  if (!shift) s->anchor = pos;
  return 0;
}

static void mulib_imtui_edit_reveal_(mulib_imtui_edit_state_t *s, const char *b,
                                     size_t budget) {
  size_t p = s->cursor;
  if (s->first > s->cursor) s->first = s->cursor;
  while (p > s->first && budget) {
    p = mulib_imtui_edit_prev_(b, p);
    --budget;
  }
  s->first = p;
}

static void mulib_imtui_edit_draw_(mulib_imtui_t *ui, mulib_imtui_rect_t rect,
                                  mulib_imtui_edit_state_t *s, const char *b,
                                  size_t n, mulib_term_style_t style, int focused) {
  mulib_imtui_rect_t clip = mulib_imtui_clip(ui);
  size_t p = s->first;
  int64_t x = rect.x, right = x + rect.w;
  size_t lo = mulib_imtui_edit_min_(s->cursor, s->anchor);
  size_t hi = mulib_imtui_edit_max_(s->cursor, s->anchor);
  mulib_term_cell_t blank = {' ', style};
  if (clip.w <= 0 || rect.y < clip.y || (int64_t)rect.y >= (int64_t)clip.y + clip.h
      || right <= clip.x || x >= (int64_t)clip.x + clip.w) return;
  if (right > (int64_t)clip.x + clip.w) right = (int64_t)clip.x + clip.w;
  mulib_imtui_fill(ui, rect, blank);
  for (; p < n && x < right; ++x) {
    mulib_term_cell_t cell = blank;
    size_t used;
    if (!mulib_utf8_decode((const unsigned char *)b + p, n - p, &cell.codepoint, &used))
      break;
    if (x >= clip.x) {
      if (p >= lo && p < hi) cell.style.attributes ^= MULIB_TERM_INVERSE;
      if (focused && p == s->cursor) cell.style.attributes |= MULIB_TERM_UNDERLINE;
      mulib_imtui_put(ui, (int)x, rect.y, cell);
    }
    p += used;
  }
  if (focused && p == s->cursor && x >= clip.x && x < right) {
    blank.style.attributes |= MULIB_TERM_UNDERLINE;
    mulib_imtui_put(ui, (int)x, rect.y, blank);
  }
}

unsigned mulib_imtui_edit(mulib_imtui_t *ui, mulib_imtui_id_t id,
                         mulib_imtui_rect_t rect, mulib_imtui_edit_state_t *s,
                         char *b, size_t *n, size_t cap,
                         mulib_term_style_t style, int enabled) {
  mulib_termkey_event_t e;
  unsigned result = 0, interaction;
  size_t p, used;
  uint32_t cp;
  if (!ui || !s || !b || !n || *n >= cap || b[*n]) return 0;
  for (p = 0; p < *n; p += used)
    if (!mulib_utf8_decode((const unsigned char *)b + p, *n - p, &cp, &used)
        || !mulib_imtui_edit_printable_(cp)) return 0;
  if (rect.w <= 0 || rect.h <= 0) {
    mulib_imtui_edit_paste_reset_(s);
    return 0;
  }
  rect.h = 1;
  s->cursor = mulib_imtui_edit_boundary_(b, *n, s->cursor);
  s->anchor = mulib_imtui_edit_boundary_(b, *n, s->anchor);
  s->first = mulib_imtui_edit_boundary_(b, *n, s->first);
  interaction = mulib_imtui_interact_ex(ui, id, rect, enabled, 0);
  while (enabled && mulib_imtui_take_key(ui, id, &e) > 0) {
    if (e.type == MULIB_TERMKEY_PASTE_BEGIN) {
      mulib_imtui_edit_paste_reset_(s); s->paste = 1;
    } else if (e.type == MULIB_TERMKEY_PASTE_END) {
      mulib_imtui_edit_paste_reset_(s);
    } else if (e.type == MULIB_TERMKEY_PASTE_TEXT && s->paste) {
      result |= mulib_imtui_edit_paste_(s, b, n, cap, &e);
    } else if (e.type == MULIB_TERMKEY_KEY) {
      /* A terminal key cannot occur within bracketed paste. Recover if the
       * widget was not submitted and the core discarded the paste end. */
      mulib_imtui_edit_paste_reset_(s);
      result |= mulib_imtui_edit_key_(s, b, n, cap, &e);
    }
  }
  if (!(interaction & MULIB_IMTUI_FOCUSED))
    mulib_imtui_edit_paste_reset_(s);
  if (!enabled) style.attributes |= MULIB_TERM_DIM;
  mulib_imtui_edit_reveal_(s, b, (size_t)rect.w - 1);
  mulib_imtui_edit_draw_(ui, rect, s, b, *n, style, enabled && (interaction & MULIB_IMTUI_FOCUSED));
  return result;
}

#ifdef __cplusplus
}
#endif
#endif /* implementation */
