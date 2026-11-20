/**
 * @file mulib-imtui-tabs.h
 * @brief Single-focus tab strip with caller-selected panel content.
 *
 * Define MULIB_IMPL_IMTUI_TABS in one translation unit.
 */
#ifndef MULIB_IMTUI_TABS_H
#define MULIB_IMTUI_TABS_H
#include "mulib-imtui-draw.h"
#ifdef __cplusplus
extern "C" {
#endif

#define MULIB_IMTUI_TABS_NONE SIZE_MAX

typedef struct mulib_imtui_tab {
  const char *label;
  size_t length;
  int enabled;
} mulib_imtui_tab_t;

typedef struct mulib_imtui_tabs_state {
  size_t first, selected;
} mulib_imtui_tabs_state_t;

/** Set the initial selected index, or MULIB_IMTUI_TABS_NONE. */
void mulib_imtui_tabs_init(mulib_imtui_tabs_state_t *, size_t selected);

/**
 * Draw the first row of rect and return 1 when input changes the selected
 * index. Left/Right and Home/End select enabled tabs without wrapping. The
 * strip scrolls horizontally by whole tabs. A tab wider than the strip is
 * clipped. An out-of-range selection is cleared without reporting a change;
 * the caller owns and draws the panel.
 */
int mulib_imtui_tabs(mulib_imtui_t *, mulib_imtui_id_t, mulib_imtui_rect_t,
                     mulib_imtui_tabs_state_t *, const mulib_imtui_tab_t *,
                     size_t count, mulib_term_style_t, int enabled);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMTUI_TABS_H */

#if defined(MULIB_IMPL_IMTUI_TABS) && !defined(MULIB_IMTUI_TABS_IMPLEMENTED)
#define MULIB_IMTUI_TABS_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif

void mulib_imtui_tabs_init(mulib_imtui_tabs_state_t *s, size_t selected) {
  if (s) { s->first = 0; s->selected = selected; }
}

static size_t mulib_imtui_tabs_width_(const mulib_imtui_tab_t *tab, size_t cap) {
  size_t width, n = tab->label ? tab->length : 0;
  /* Only the first cap scalars can affect placement; at most four bytes
   * per scalar. Counting invalid bytes more cheaply beyond cap is harmless. */
  if (cap <= SIZE_MAX / 4 && n > cap * 4) return cap;
  width = mulib_imtui_text_width(tab->label, n);
  return cap < 2 || width > cap - 2 ? cap : width + 2;
}

static size_t mulib_imtui_tabs_search_(const mulib_imtui_tab_t *tabs,
                                      size_t count, size_t pos, int backward) {
  if (backward) {
    while (pos) {
      --pos;
      if (tabs[pos].enabled) return pos;
    }
  } else {
    for (; pos < count; ++pos)
      if (tabs[pos].enabled) return pos;
  }
  return MULIB_IMTUI_TABS_NONE;
}

static void mulib_imtui_tabs_reveal_(mulib_imtui_tabs_state_t *s,
                                     const mulib_imtui_tab_t *tabs, size_t width) {
  if (s->selected == MULIB_IMTUI_TABS_NONE) return;
  if (s->selected < s->first) { s->first = s->selected; return; }
  for (size_t i = s->first; i < s->selected; ++i) {
    width -= mulib_imtui_tabs_width_(tabs + i, width);
    if (!width) { s->first = s->selected; return; }
    --width; /* gap */
    if (!width) { s->first = s->selected; return; }
  }
  if (s->first != s->selected
      && mulib_imtui_tabs_width_(tabs + s->selected, width + 1) > width)
    s->first = s->selected;
}

static void mulib_imtui_tabs_draw_(mulib_imtui_t *ui, mulib_imtui_rect_t rect,
                                   const mulib_imtui_tabs_state_t *s,
                                   const mulib_imtui_tab_t *tabs, size_t count,
                                   mulib_term_style_t style, int enabled,
                                   unsigned interaction) {
  mulib_imtui_rect_t clip = mulib_imtui_clip(ui);
  int64_t x = rect.x, right = x + rect.w, clip_right = (int64_t)clip.x + clip.w;
  mulib_term_cell_t blank = {' ', style};
  if (clip.w <= 0 || rect.y < clip.y || (int64_t)rect.y >= (int64_t)clip.y + clip.h
      || right <= clip.x || x >= clip_right) return;
  if (!enabled) blank.style.attributes |= MULIB_TERM_DIM;
  if (s->selected == MULIB_IMTUI_TABS_NONE && (interaction & MULIB_IMTUI_FOCUSED))
    blank.style.attributes |= MULIB_TERM_UNDERLINE;
  mulib_imtui_fill(ui, rect, blank);
  for (size_t i = s->first; i < count && x < right && x < clip_right; ++i) {
    const mulib_imtui_tab_t *tab = tabs + i;
    int width = (int)mulib_imtui_tabs_width_(tab, (size_t)(right - x));
    int64_t far = x + width;
    if (far > clip.x) {
      mulib_imtui_rect_t part = {(int)x, rect.y, width, 1};
      mulib_term_cell_t cell = blank;
      if (!enabled || !tab->enabled) cell.style.attributes |= MULIB_TERM_DIM;
      if (i == s->selected) {
        cell.style.attributes ^= MULIB_TERM_INVERSE;
        if (interaction & MULIB_IMTUI_FOCUSED) cell.style.attributes |= MULIB_TERM_UNDERLINE;
      }
      mulib_imtui_fill(ui, part, cell);
      if (tab->label && mulib_imtui_clip_push(ui, part)) {
        int padded = width > 2;
        mulib_imtui_text(ui, part.x + padded, part.y, tab->label, tab->length, cell.style);
        mulib_imtui_clip_pop(ui);
      }
    }
    x = far + 1;
  }
}

int mulib_imtui_tabs(mulib_imtui_t *ui, mulib_imtui_id_t id,
                     mulib_imtui_rect_t rect, mulib_imtui_tabs_state_t *s,
                     const mulib_imtui_tab_t *tabs, size_t count,
                     mulib_term_style_t style, int enabled) {
  unsigned interaction;
  int changed = 0;
  mulib_termkey_event_t e;
  if (!s) return 0;
  if (!tabs) count = 0;
  if (s->selected >= count) s->selected = MULIB_IMTUI_TABS_NONE;
  if (s->first >= count) s->first = 0;
  if (!ui || rect.w <= 0 || rect.h <= 0) return 0;
  rect.h = 1;
  interaction = mulib_imtui_interact_ex(ui, id, rect, enabled && count != 0, 0);
  while (enabled && count && mulib_imtui_take_key(ui, id, &e) > 0) {
    size_t next;
    if (e.type != MULIB_TERMKEY_KEY || e.modifiers) continue;
    switch (e.key) {
      case MULIB_TERMKEY_LEFT:
        next = mulib_imtui_tabs_search_(tabs, count,
                 s->selected == MULIB_IMTUI_TABS_NONE ? count : s->selected, 1);
        break;
      case MULIB_TERMKEY_RIGHT:
        next = mulib_imtui_tabs_search_(tabs, count,
                 s->selected == MULIB_IMTUI_TABS_NONE ? 0 : s->selected + 1, 0);
        break;
      case MULIB_TERMKEY_HOME: next = mulib_imtui_tabs_search_(tabs, count, 0, 0); break;
      case MULIB_TERMKEY_END: next = mulib_imtui_tabs_search_(tabs, count, count, 1); break;
      default: continue;
    }
    if (next != MULIB_IMTUI_TABS_NONE && s->selected != next) {
      s->selected = next; changed = 1;
    }
  }
  mulib_imtui_tabs_reveal_(s, tabs, (size_t)rect.w);
  mulib_imtui_tabs_draw_(ui, rect, s, tabs, count, style, enabled, interaction);
  return changed;
}

#ifdef __cplusplus
}
#endif
#endif /* implementation */
