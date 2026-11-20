/**
 * @file mulib-imtui-widgets.h
 * @brief Allocation-free immediate widgets for the imtui cell canvas.
 *
 * Define MULIB_IMPL_IMTUI_WIDGETS in one translation unit; link the imtui
 * core, draw, and UTF-8 implementations. Submit widgets between begin/end
 * using stable, unique, nonzero IDs. Labels are borrowed UTF-8, shown on one
 * row up to the first CR or LF. Caller-owned values comprise all persistent
 * widget state. A NULL theme selects a color-independent default palette.
 */
#ifndef MULIB_IMTUI_WIDGETS_H
#define MULIB_IMTUI_WIDGETS_H
#include "mulib-imtui-draw.h"
#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief Per-call styles; copy the default palette to customize it.
   * Control style priority is disabled, pressed/held, focused, hovered,
   * selected, normal. Selection markers do not depend on style. */
  typedef struct mulib_imtui_theme
  {
    mulib_term_style_t normal, hovered, focused, pressed, disabled, selected;
    mulib_term_style_t separator, progress_empty, progress_full;
    int unicode; /**< Use Unicode for rules and button borders if nonzero. */
  } mulib_imtui_theme_t;

  /** @brief Terminal-default colors, with distinct focus and disabled states. */
  mulib_imtui_theme_t mulib_imtui_theme_default (void);
  /** @brief Noninteractive left-aligned label at the top of the rectangle. */
  void mulib_imtui_label (mulib_imtui_t *ui, mulib_imtui_rect_t rect,
                         const char *text, size_t length,
                         const mulib_imtui_theme_t *theme);
  /** @brief 1 on activation, 0 otherwise. Enter/Space or left-click follows
   * the core's interaction rules. Disabled controls draw but cannot activate. */
  int mulib_imtui_button (mulib_imtui_t *ui, mulib_imtui_id_t id,
                         mulib_imtui_rect_t rect, const char *text,
                         size_t length, int enabled);
  int mulib_imtui_button_themed (mulib_imtui_t *ui, mulib_imtui_id_t id,
                                mulib_imtui_rect_t rect, const char *text,
                                size_t length, int enabled,
                                const mulib_imtui_theme_t *theme);
  /** @brief Toggle a caller-owned boolean (zero/nonzero); return 1 if changed.
   * A NULL value renders an unchecked, disabled control. */
  int mulib_imtui_checkbox (mulib_imtui_t *ui, mulib_imtui_id_t id,
                           mulib_imtui_rect_t rect, const char *text,
                           size_t length, int *value, int enabled);
  int mulib_imtui_checkbox_themed (mulib_imtui_t *ui, mulib_imtui_id_t id,
                                  mulib_imtui_rect_t rect, const char *text,
                                  size_t length, int *value, int enabled,
                                  const mulib_imtui_theme_t *theme);
  /** @brief One radio option. Share selection across options; this widget
   * writes option on activation and returns 1 only when selection changes.
   * Each option has its own ID and participates in Tab navigation. Options
   * submitted earlier in the frame repaint with the new selection next frame. */
  int mulib_imtui_radio (mulib_imtui_t *ui, mulib_imtui_id_t id,
                        mulib_imtui_rect_t rect, const char *text,
                        size_t length, int option, int *selection,
                        int enabled, const mulib_imtui_theme_t *theme);
  /** @brief Selectable row. Return activation; caller manages selection. */
  int mulib_imtui_selectable (mulib_imtui_t *ui, mulib_imtui_id_t id,
                             mulib_imtui_rect_t rect, const char *text,
                             size_t length, int selected, int enabled,
                             const mulib_imtui_theme_t *theme);
  /** @brief Inclusive integer slider. Left/Down and Right/Up move one step;
   * Home/End select the bounds. Mouse press/drag uses the frame's final
   * pointer position, clamped to the track; pointer input takes precedence
   * over keys when both adjust the slider in one frame.
   * A one-column track supports keyboard adjustment only. Return 1 only if
   * input changed *value. An out-of-range value is visually clamped without
   * being written merely by drawing. NULL value, minimum >= maximum or
   * disabled makes the control noninteractive. A second row, if available,
   * displays the current value in decimal. */
  int mulib_imtui_slider (mulib_imtui_t *ui, mulib_imtui_id_t id,
                         mulib_imtui_rect_t rect, int *value, int minimum,
                         int maximum, int enabled,
                         const mulib_imtui_theme_t *theme);
  /** @brief Noninteractive bar; value clamps to maximum, maximum zero is
   * empty. Filled columns round down, full completion fills the rectangle. */
  void mulib_imtui_progress (mulib_imtui_t *ui, mulib_imtui_rect_t rect,
                             unsigned value, unsigned maximum,
                             const mulib_imtui_theme_t *theme);
  /** @brief Noninteractive rule at the top or left edge of rect. */
  void mulib_imtui_separator (mulib_imtui_t *ui, mulib_imtui_rect_t rect,
                              int vertical, const mulib_imtui_theme_t *theme);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMTUI_WIDGETS_H */

#if defined(MULIB_IMPL_IMTUI_WIDGETS) && !defined(MULIB_IMTUI_WIDGETS_IMPLEMENTED)
#define MULIB_IMTUI_WIDGETS_IMPLEMENTED
#include <limits.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C"
{
#endif

  static const mulib_imtui_theme_t mulib_imtui_widgets_default_ = {
    { MULIB_TERM_COLOR_DEFAULT, MULIB_TERM_COLOR_DEFAULT, 0 },
    { MULIB_TERM_COLOR_DEFAULT, MULIB_TERM_COLOR_DEFAULT, MULIB_TERM_BOLD },
    { MULIB_TERM_COLOR_DEFAULT, MULIB_TERM_COLOR_DEFAULT,
      MULIB_TERM_INVERSE | MULIB_TERM_UNDERLINE },
    { MULIB_TERM_COLOR_DEFAULT, MULIB_TERM_COLOR_DEFAULT,
      MULIB_TERM_INVERSE | MULIB_TERM_UNDERLINE | MULIB_TERM_BOLD },
    { MULIB_TERM_COLOR_DEFAULT, MULIB_TERM_COLOR_DEFAULT, MULIB_TERM_DIM },
    { MULIB_TERM_COLOR_DEFAULT, MULIB_TERM_COLOR_DEFAULT, MULIB_TERM_BOLD },
    { MULIB_TERM_COLOR_DEFAULT, MULIB_TERM_COLOR_DEFAULT, 0 },
    { MULIB_TERM_COLOR_DEFAULT, MULIB_TERM_COLOR_DEFAULT, 0 },
    { MULIB_TERM_COLOR_DEFAULT, MULIB_TERM_COLOR_DEFAULT, MULIB_TERM_BOLD },
    0
  };

  mulib_imtui_theme_t
  mulib_imtui_theme_default (void)
  {
    return mulib_imtui_widgets_default_;
  }

  static const mulib_imtui_theme_t *
  mulib_imtui_widgets_theme_ (const mulib_imtui_theme_t *theme)
  {
    return theme ? theme : &mulib_imtui_widgets_default_;
  }

  static mulib_term_style_t
  mulib_imtui_widgets_style_ (const mulib_imtui_theme_t *theme, unsigned state,
                             int enabled, int selected)
  {
    if (!enabled)
      return theme->disabled;
    if (state & (MULIB_IMTUI_PRESSED | MULIB_IMTUI_HELD))
      return theme->pressed;
    if (state & MULIB_IMTUI_FOCUSED)
      return theme->focused;
    if (state & MULIB_IMTUI_HOVERED)
      return theme->hovered;
    return selected ? theme->selected : theme->normal;
  }

  static void
  mulib_imtui_widgets_fill_ (mulib_imtui_t *ui, mulib_imtui_rect_t r,
                            uint32_t cp, mulib_term_style_t style)
  {
    mulib_term_cell_t cell = { cp, style };
    mulib_imtui_fill (ui, r, cell);
  }

  /* All offsets below are nonnegative. A cell beyond INT_MAX can never
   * intersect a representable canvas, so don't fold it into that canvas. */
  static int
  mulib_imtui_widgets_offset_ (int *coordinate, int offset)
  {
    if (*coordinate > INT_MAX - offset)
      return 0;
    *coordinate += offset;
    return 1;
  }

  static void
  mulib_imtui_widgets_line_ (mulib_imtui_t *ui, mulib_imtui_rect_t r,
                            const char *text, size_t n,
                            mulib_term_style_t style, int center)
  {
    mulib_imtui_rect_t clip;
    size_t length = 0;
    int width = 0;
    if (!ui || !text || !n || r.w <= 0 || r.h <= 0)
      return;
    clip = mulib_imtui_clip (ui);
    if (clip.w <= 0 || clip.h <= 0 || r.y < clip.y
        || (int64_t)r.y >= (int64_t)clip.y + clip.h
        || (int64_t)r.x >= (int64_t)clip.x + clip.w
        || (int64_t)r.x + r.w <= clip.x)
      return;
    /* Match draw's one-cell-per-scalar measure, including malformed bytes.
     * Bound text to the row without consuming another clip-stack slot. */
    while (length < n && width < r.w && text[length] != '\n'
           && text[length] != '\r')
      {
        uint32_t cp;
        size_t used;
        if (!mulib_utf8_decode ((const unsigned char *)text + length,
                                n - length, &cp, &used))
          used = 1;
        length += used;
        ++width;
      }
    if (!length)
      return;
    if (center && !mulib_imtui_widgets_offset_ (&r.x, (r.w - width) / 2))
      return;
    mulib_imtui_text (ui, r.x, r.y, text, length, style);
  }

  void
  mulib_imtui_label (mulib_imtui_t *ui, mulib_imtui_rect_t r,
                    const char *text, size_t n, const mulib_imtui_theme_t *theme)
  {
    mulib_imtui_widgets_line_ (ui, r, text, n,
                              mulib_imtui_widgets_theme_ (theme)->normal, 0);
  }

  int
  mulib_imtui_button_themed (mulib_imtui_t *ui, mulib_imtui_id_t id,
                            mulib_imtui_rect_t r, const char *text, size_t n,
                            int enabled, const mulib_imtui_theme_t *theme)
  {
    unsigned state;
    int boxed;
    mulib_term_style_t style;
    if (!ui || r.w <= 0 || r.h <= 0)
      return 0;
    theme = mulib_imtui_widgets_theme_ (theme);
    state = mulib_imtui_interact (ui, id, r, enabled);
    style = mulib_imtui_widgets_style_ (theme, state, enabled, 0);
    mulib_imtui_widgets_fill_ (ui, r, ' ', style);
    boxed = r.h >= 3 && r.w >= 3;
    if (boxed)
      mulib_imtui_box (ui, r, style, theme->unicode);
    if (mulib_imtui_widgets_offset_ (&r.y, (r.h - 1) / 2))
      {
        r.h = 1;
        if (r.w >= 3)
          {
            if (!boxed)
              {
                mulib_imtui_rect_t edge = r;
                edge.w = 1;
                mulib_imtui_widgets_fill_ (ui, edge, '[', style);
                if (mulib_imtui_widgets_offset_ (&edge.x, r.w - 1))
                  mulib_imtui_widgets_fill_ (ui, edge, ']', style);
              }
            if (!mulib_imtui_widgets_offset_ (&r.x, 1))
              return enabled && (state & MULIB_IMTUI_ACTIVATED) != 0;
            r.w -= 2;
          }
        mulib_imtui_widgets_line_ (ui, r, text, n, style, 1);
      }
    return enabled && (state & MULIB_IMTUI_ACTIVATED) != 0;
  }

  int
  mulib_imtui_button (mulib_imtui_t *ui, mulib_imtui_id_t id,
                     mulib_imtui_rect_t r, const char *text, size_t n,
                     int enabled)
  {
    return mulib_imtui_button_themed (ui, id, r, text, n, enabled, NULL);
  }

  static void
  mulib_imtui_widgets_option_ (mulib_imtui_t *ui, mulib_imtui_rect_t r,
                               const char *text, size_t n, const char *mark,
                               int mark_width, uint32_t compact,
                               mulib_term_style_t style)
  {
    mulib_imtui_widgets_fill_ (ui, r, ' ', style);
    if (!mulib_imtui_widgets_offset_ (&r.y, (r.h - 1) / 2))
      return;
    r.h = 1;
    if (r.w < mark_width)
      {
        if (r.w == mark_width - 1 && r.w > 1)
          {
            /* Keep the brackets; omit only the trailing space. */
            mulib_imtui_widgets_line_ (ui, r, mark, (size_t)r.w, style, 0);
            return;
          }
        mark_width = 1;
        {
          mulib_imtui_rect_t prefix = r;
          prefix.w = 1;
          mulib_imtui_widgets_fill_ (ui, prefix, compact, style);
        }
      }
    else
      mulib_imtui_widgets_line_ (ui, r, mark, (size_t)mark_width, style, 0);
    if (mulib_imtui_widgets_offset_ (&r.x, mark_width))
      {
        r.w -= mark_width;
        mulib_imtui_widgets_line_ (ui, r, text, n, style, 0);
      }
  }

  int
  mulib_imtui_checkbox_themed (mulib_imtui_t *ui, mulib_imtui_id_t id,
                              mulib_imtui_rect_t r, const char *text, size_t n,
                              int *value, int enabled,
                              const mulib_imtui_theme_t *theme)
  {
    int changed, checked;
    unsigned state;
    if (!ui || r.w <= 0 || r.h <= 0)
      return 0;
    enabled = enabled && value != NULL;
    state = mulib_imtui_interact (ui, id, r, enabled);
    changed = enabled && (state & MULIB_IMTUI_ACTIVATED) != 0;
    if (changed)
      *value = !*value;
    checked = value && *value != 0;
    theme = mulib_imtui_widgets_theme_ (theme);
    mulib_imtui_widgets_option_ (
        ui, r, text, n, checked ? "[x] " : "[ ] ", 4, checked ? 'x' : '-',
        mulib_imtui_widgets_style_ (theme, state, enabled, checked));
    return changed;
  }

  int
  mulib_imtui_checkbox (mulib_imtui_t *ui, mulib_imtui_id_t id,
                       mulib_imtui_rect_t r, const char *text, size_t n,
                       int *value, int enabled)
  {
    return mulib_imtui_checkbox_themed (ui, id, r, text, n, value, enabled,
                                       NULL);
  }

  int
  mulib_imtui_radio (mulib_imtui_t *ui, mulib_imtui_id_t id,
                    mulib_imtui_rect_t r, const char *text, size_t n,
                    int option, int *selection, int enabled,
                    const mulib_imtui_theme_t *theme)
  {
    int changed, selected;
    unsigned state;
    if (!ui || r.w <= 0 || r.h <= 0)
      return 0;
    enabled = enabled && selection != NULL;
    state = mulib_imtui_interact (ui, id, r, enabled);
    changed = enabled && (state & MULIB_IMTUI_ACTIVATED) && *selection != option;
    if (changed)
      *selection = option;
    selected = selection && *selection == option;
    theme = mulib_imtui_widgets_theme_ (theme);
    mulib_imtui_widgets_option_ (
        ui, r, text, n, selected ? "(*) " : "( ) ", 4, selected ? '*' : 'o',
        mulib_imtui_widgets_style_ (theme, state, enabled, selected));
    return changed;
  }

  int
  mulib_imtui_selectable (mulib_imtui_t *ui, mulib_imtui_id_t id,
                         mulib_imtui_rect_t r, const char *text, size_t n,
                         int selected, int enabled,
                         const mulib_imtui_theme_t *theme)
  {
    unsigned state;
    if (!ui || r.w <= 0 || r.h <= 0)
      return 0;
    state = mulib_imtui_interact (ui, id, r, enabled);
    theme = mulib_imtui_widgets_theme_ (theme);
    mulib_imtui_widgets_option_ (
        ui, r, text, n, selected ? "> " : "  ", 2, selected ? '>' : ' ',
        mulib_imtui_widgets_style_ (theme, state, enabled, selected));
    return enabled && (state & MULIB_IMTUI_ACTIVATED) != 0;
  }

  static int
  mulib_imtui_widgets_clamp_ (int value, int lo, int hi)
  {
    return value < lo ? lo : value > hi ? hi : value;
  }

  static int
  mulib_imtui_widgets_slider_pointer_ (mulib_imtui_rect_t r, size_t x,
                                       int lo, int hi)
  {
    uint64_t range = (uint64_t)((int64_t)hi - lo);
    unsigned cells = (unsigned)(r.w - 1), col;
    int64_t right = (int64_t)r.x + cells;
    /* Pointer coordinates are unsigned and may exceed INT64_MAX. Clamp
     * before converting, including when the entire track is left of zero. */
    if (right <= 0 || x >= (uint64_t)right)
      return hi;
    if (r.x >= 0 && x <= (unsigned)r.x)
      return lo;
    col = (unsigned)((int64_t)x - r.x);
    return (int)((int64_t)lo + (int64_t)((range * col + cells / 2) / cells));
  }

  static void
  mulib_imtui_widgets_slider_draw_ (mulib_imtui_t *ui, mulib_imtui_rect_t r,
                                    int value, int lo, int hi,
                                    mulib_term_style_t style)
  {
    mulib_imtui_rect_t track = r;
    mulib_imtui_widgets_fill_ (ui, r, ' ', style);
    track.h = 1;
    mulib_imtui_widgets_fill_ (ui, track, '-', style);
    if (lo <= hi)
      {
        int thumb = 0;
        if (lo < hi)
          {
            uint64_t range = (uint64_t)((int64_t)hi - lo);
            uint64_t offset = (uint64_t)((int64_t)value - lo);
            thumb = (int)((offset * (unsigned)(r.w - 1) + range / 2) / range);
          }
        if (thumb)
          {
            track.w = thumb;
            mulib_imtui_widgets_fill_ (ui, track, '=', style);
          }
        if (mulib_imtui_widgets_offset_ (&track.x, thumb))
          {
            track.w = 1;
            mulib_imtui_widgets_fill_ (ui, track, 'O', style);
          }
        if (r.h > 1 && mulib_imtui_widgets_offset_ (&r.y, 1))
          {
            char number[sizeof (int) * CHAR_BIT + 2];
            int n = snprintf (number, sizeof (number), "%d", value);
            if (n > 0 && (size_t)n < sizeof (number))
              mulib_imtui_widgets_line_ (ui, r, number, (size_t)n, style, 1);
          }
      }
  }

  static int
  mulib_imtui_widgets_slider_key_ (const mulib_termkey_event_t *event,
                                   int *value, int lo, int hi)
  {
    if (event->type != MULIB_TERMKEY_KEY
        || (event->modifiers & ~(unsigned)MULIB_TERMKEY_SHIFT))
      return 0;
    switch (event->key)
      {
      case MULIB_TERMKEY_LEFT:
      case MULIB_TERMKEY_DOWN:
        if (*value > lo)
          --*value;
        return 1;
      case MULIB_TERMKEY_RIGHT:
      case MULIB_TERMKEY_UP:
        if (*value < hi)
          ++*value;
        return 1;
      case MULIB_TERMKEY_HOME:
        *value = lo;
        return 1;
      case MULIB_TERMKEY_END:
        *value = hi;
        return 1;
      default:
        return 0;
      }
  }

  int
  mulib_imtui_slider (mulib_imtui_t *ui, mulib_imtui_id_t id,
                     mulib_imtui_rect_t r, int *value, int lo, int hi,
                     int enabled, const mulib_imtui_theme_t *theme)
  {
    unsigned state;
    int next, changed = 0;
    mulib_termkey_event_t event;
    if (!ui || r.w <= 0 || r.h <= 0)
      return 0;
    enabled = enabled && value != NULL && lo < hi;
    state = mulib_imtui_interact_ex (ui, id, r, enabled, 0);
    next = value && lo <= hi ? mulib_imtui_widgets_clamp_ (*value, lo, hi) : lo;
    if (enabled)
      {
        int adjusted = 0;
        if (r.w > 1 && ui->mouse_known
            && (state & (MULIB_IMTUI_PRESSED | MULIB_IMTUI_HELD
                         | MULIB_IMTUI_ACTIVATED)))
          {
            next = mulib_imtui_widgets_slider_pointer_ (r, ui->mouse_x, lo, hi);
            adjusted = 1;
          }
        else
          while (mulib_imtui_take_key (ui, id, &event) > 0)
            adjusted |= mulib_imtui_widgets_slider_key_ (&event, &next, lo, hi);
        if (adjusted && next != *value)
          {
            *value = next;
            changed = 1;
          }
      }
    theme = mulib_imtui_widgets_theme_ (theme);
    mulib_imtui_widgets_slider_draw_ (
        ui, r, next, lo, hi, mulib_imtui_widgets_style_ (theme, state, enabled, 0));
    return changed;
  }

  void
  mulib_imtui_progress (mulib_imtui_t *ui, mulib_imtui_rect_t r,
                       unsigned value, unsigned maximum,
                       const mulib_imtui_theme_t *theme)
  {
    int columns;
    if (!ui || r.w <= 0 || r.h <= 0)
      return;
    columns = !maximum ? 0 : value >= maximum ? r.w
                        : (int)((uint64_t)value * (unsigned)r.w / maximum);
    theme = mulib_imtui_widgets_theme_ (theme);
    if (columns > 0)
      {
        mulib_imtui_rect_t full = r;
        full.w = columns;
        mulib_imtui_widgets_fill_ (ui, full, '#', theme->progress_full);
      }
    if (columns < r.w && mulib_imtui_widgets_offset_ (&r.x, columns))
      {
        r.w -= columns;
        mulib_imtui_widgets_fill_ (ui, r, '.', theme->progress_empty);
      }
  }

  void
  mulib_imtui_separator (mulib_imtui_t *ui, mulib_imtui_rect_t r,
                        int vertical, const mulib_imtui_theme_t *theme)
  {
    uint32_t cp;
    if (!ui || r.w <= 0 || r.h <= 0)
      return;
    theme = mulib_imtui_widgets_theme_ (theme);
    if (vertical)
      {
        r.w = 1;
        cp = theme->unicode ? 0x2502u : '|';
      }
    else
      {
        r.h = 1;
        cp = theme->unicode ? 0x2500u : '-';
      }
    mulib_imtui_widgets_fill_ (ui, r, cp, theme->separator);
  }

#ifdef __cplusplus
}
#endif
#endif /* implementation */
