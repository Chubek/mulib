/**
 * @file mulib-imtui-list.h
 * @brief Selectable virtual viewport over caller-owned list rows.
 *
 * Define MULIB_IMPL_IMTUI_LIST in one translation unit. No row allocation or
 * iteration through off-screen data is needed, regardless of list length.
 */
#ifndef MULIB_IMTUI_LIST_H
#define MULIB_IMTUI_LIST_H
#include "mulib-imtui-draw.h"
#ifdef __cplusplus
extern "C" {
#endif

#define MULIB_IMTUI_LIST_NONE SIZE_MAX

typedef struct mulib_imtui_list_state {
  size_t first, selected;
  /** Last activated row this call, or MULIB_IMTUI_LIST_NONE. */
  size_t activated;
} mulib_imtui_list_state_t;

enum mulib_imtui_list_result {
  MULIB_IMTUI_LIST_CHANGED = 1u,
  MULIB_IMTUI_LIST_ACTIVATED = 2u
};

/** Initialize with no selection and the first row at the top. */
void mulib_imtui_list_init(mulib_imtui_list_state_t *);

/**
 * A visible row painter. The row rectangle has height one and the current
 * clip is constrained to it. Use this callback for drawing, not interaction.
 */
typedef void (*mulib_imtui_list_row_fn)(mulib_imtui_t *, mulib_imtui_rect_t,
                                         size_t row, mulib_term_style_t, void *);

/** A length-delimited label borrowed until the next callback. NULL is empty. */
typedef const char *(*mulib_imtui_list_label_fn)(void *, size_t row, size_t *length);

/**
 * Show one row per terminal line. Up/Down, Home/End and Page Up/Down move the
 * selection and reveal it; Enter activates it. When the row count shrinks,
 * first is clamped and missing selections are cleared without reporting input.
 * The caller may set first/selected between frames. The callback is invoked
 * once per visible data row, in ascending order, after input is processed.
 */
unsigned mulib_imtui_list_view(mulib_imtui_t *, mulib_imtui_id_t,
                              mulib_imtui_rect_t, mulib_imtui_list_state_t *,
                              size_t count, mulib_imtui_list_row_fn, void *,
                              mulib_term_style_t, int enabled);

/** As list_view, with a UTF-8 label on each row. */
unsigned mulib_imtui_list(mulib_imtui_t *, mulib_imtui_id_t, mulib_imtui_rect_t,
                         mulib_imtui_list_state_t *, size_t count,
                         mulib_imtui_list_label_fn, void *,
                         mulib_term_style_t, int enabled);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMTUI_LIST_H */

#if defined(MULIB_IMPL_IMTUI_LIST) && !defined(MULIB_IMTUI_LIST_IMPLEMENTED)
#define MULIB_IMTUI_LIST_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif

void mulib_imtui_list_init(mulib_imtui_list_state_t *s) {
  if (!s) return;
  s->first = 0;
  s->activated = s->selected = MULIB_IMTUI_LIST_NONE;
}

static void mulib_imtui_list_reveal_(mulib_imtui_list_state_t *s, size_t height) {
  if (s->selected < s->first) s->first = s->selected;
  else if (s->selected - s->first >= height) s->first = s->selected - height + 1;
}

static unsigned mulib_imtui_list_key_(mulib_imtui_list_state_t *s, size_t count,
                                      size_t height, const mulib_termkey_event_t *e) {
  size_t before = s->selected, next = before, last = count - 1;
  int has = next != MULIB_IMTUI_LIST_NONE;
  if (e->type != MULIB_TERMKEY_KEY || e->modifiers) return 0;
  switch (e->key) {
    case MULIB_TERMKEY_DOWN: next = has ? next + (next < last) : 0; break;
    case MULIB_TERMKEY_UP: next = has ? next - (next > 0) : last; break;
    case MULIB_TERMKEY_HOME: next = 0; break;
    case MULIB_TERMKEY_END: next = last; break;
    case MULIB_TERMKEY_PAGE_UP: next = has && next > height ? next - height : 0; break;
    case MULIB_TERMKEY_PAGE_DOWN:
      next = !has ? 0 : last - next < height ? last : next + height;
      break;
    case MULIB_TERMKEY_ENTER:
      if (has) { s->activated = next; return MULIB_IMTUI_LIST_ACTIVATED; }
      return 0;
    default: return 0;
  }
  s->selected = next;
  mulib_imtui_list_reveal_(s, height);
  return next != before ? MULIB_IMTUI_LIST_CHANGED : 0;
}

unsigned mulib_imtui_list_view(mulib_imtui_t *ui, mulib_imtui_id_t id,
                              mulib_imtui_rect_t rect, mulib_imtui_list_state_t *s,
                              size_t count, mulib_imtui_list_row_fn paint, void *user,
                              mulib_term_style_t style, int enabled) {
  size_t height, limit;
  unsigned state, result = 0;
  mulib_termkey_event_t e;
  mulib_imtui_rect_t clip;
  mulib_term_cell_t blank;
  int64_t y, end;
  if (!s) return 0;
  s->activated = MULIB_IMTUI_LIST_NONE;
  if (s->selected >= count) s->selected = MULIB_IMTUI_LIST_NONE;
  height = rect.h > 0 ? (size_t)rect.h : 1;
  limit = count > height ? count - height : 0;
  if (s->first > limit) s->first = limit;
  if (!ui || rect.w <= 0 || rect.h <= 0) return 0;
  state = mulib_imtui_interact_ex(ui, id, rect, enabled && count != 0, 0);
  if (enabled && count)
    while (mulib_imtui_take_key(ui, id, &e) > 0)
      result |= mulib_imtui_list_key_(s, count, height, &e);
  blank.codepoint = ' ';
  if (!enabled) style.attributes |= MULIB_TERM_DIM;
  blank.style = style;
  mulib_imtui_fill(ui, rect, blank);
  if (!count || !mulib_imtui_clip_push(ui, rect)) return result;
  clip = mulib_imtui_clip(ui);
  end = (int64_t)clip.y + clip.h;
  if (clip.w > 0)
    for (y = clip.y; y < end; ++y) {
      size_t offset = (size_t)(y - rect.y), index;
      mulib_imtui_rect_t row = {rect.x, (int)y, rect.w, 1};
      mulib_term_cell_t cell = blank;
      if (offset >= count - s->first) break;
      index = s->first + offset;
      if (index == s->selected) {
        cell.style.attributes ^= MULIB_TERM_INVERSE;
        if (state & MULIB_IMTUI_FOCUSED) cell.style.attributes |= MULIB_TERM_UNDERLINE;
        mulib_imtui_fill(ui, row, cell);
      }
      if (paint && mulib_imtui_clip_push(ui, row)) {
        paint(ui, row, index, cell.style, user);
        mulib_imtui_clip_pop(ui);
      }
    }
  mulib_imtui_clip_pop(ui);
  return result;
}

typedef struct mulib_imtui_list_label_context_ {
  mulib_imtui_list_label_fn label;
  void *user;
} mulib_imtui_list_label_context_;

static void mulib_imtui_list_label_(mulib_imtui_t *ui, mulib_imtui_rect_t row,
                                    size_t i, mulib_term_style_t style, void *user) {
  mulib_imtui_list_label_context_ *context = (mulib_imtui_list_label_context_ *)user;
  size_t n = 0;
  const char *s = context->label(context->user, i, &n);
  if (s) mulib_imtui_text(ui, row.x, row.y, s, n, style);
}

unsigned mulib_imtui_list(mulib_imtui_t *ui, mulib_imtui_id_t id,
                         mulib_imtui_rect_t rect, mulib_imtui_list_state_t *s,
                         size_t count, mulib_imtui_list_label_fn label, void *user,
                         mulib_term_style_t style, int enabled) {
  mulib_imtui_list_label_context_ context = {label, user};
  return mulib_imtui_list_view(ui, id, rect, s, count,
                               label ? mulib_imtui_list_label_ : NULL, &context, style, enabled);
}

#ifdef __cplusplus
}
#endif
#endif /* implementation */
