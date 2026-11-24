/**
 * @file mulib-imtui-render.h
 * @brief Differential terminal output and plain UTF-8 snapshots of an ImTUI canvas.
 *
 * Define MULIB_IMPL_IMTUI_RENDER in one translation unit. Requires the ImTUI
 * core and terminfo, termstyle, strbuf, UTF-8 and shared terminal cell
 * implementations. No terminal detection or I/O.
 */
#ifndef MULIB_IMTUI_RENDER_H
#define MULIB_IMTUI_RENDER_H
#include "mulib-imtui-core.h"
#include "mulib-termstyle.h"
#ifdef __cplusplus
extern "C" {
#endif

/** @brief Owned baseline for one output stream. Initialize before use;
 * do not copy or mutate members. */
typedef struct mulib_imtui_renderer
{
  mulib_term_cell_t *baseline;
  size_t capacity;
  int width, height, valid;
  unsigned flags;
  mulib_terminfo_t profile;
} mulib_imtui_renderer_t;

/** @brief Initialize a fresh renderer (does not free an existing renderer). */
void mulib_imtui_renderer_init(mulib_imtui_renderer_t *);
/** @brief Free renderer storage and zero the object; NULL is allowed. */
void mulib_imtui_renderer_free(mulib_imtui_renderer_t *);
/** @brief Force the next interactive render to repaint the full canvas. */
void mulib_imtui_renderer_invalidate(mulib_imtui_renderer_t *);

/** @brief Append a frame or differential update atomically, 1 success/0 failure.
 *
 * Interactive output uses a trusted terminfo profile (non-NULL; callers may
 * build the explicit ANSI profile). Uses cup to position changed spans, sc/rc
 * to save/restore the terminal cursor (using the terminal's saved-cursor
 * slot), and termstyle for rendition. Assumes the canvas starts at terminal
 * (0,0), fits the terminal, and the terminal's usual cursor addressing and
 * wrap modes are active. It does not clear outside the current canvas on
 * resize. Cells containing wide, nonspacing, or grapheme-clustering scalars
 * may not line up on a real terminal (one scalar per logical cell).
 *
 * Only a successful append advances the interactive baseline. Keep and drain
 * all queued bytes in order, even if socket/terminal writes are partial; if a
 * queue is discarded or another writer changes the screen, invalidate before
 * repainting. Recover terminal modes first if a control sequence was cut off.
 * Canvas size, terminfo profile and flag changes repaint it fully.
 * Unsupported or failed capability expansions, or an update exceeding 64 MiB,
 * fail without appending bytes.
 *
 * Flags are MULIB_TERMSTYLE_TRUECOLOR and MULIB_TERMSTYLE_PLAIN. PLAIN emits
 * an entire escape-free UTF-8 snapshot, with one LF after each row including
 * the last, ignores the profile, and does not modify the interactive baseline.
 * NULL is allowed for both renderer and profile in PLAIN mode.
 */
int mulib_imtui_render(mulib_imtui_renderer_t *, const mulib_imtui_t *,
                      const mulib_terminfo_t *, unsigned flags, mulib_strbuf_t *);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_IMTUI_RENDER) && !defined(MULIB_IMTUI_RENDER_IMPLEMENTED)
#define MULIB_IMTUI_RENDER_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

void
mulib_imtui_renderer_init(mulib_imtui_renderer_t *renderer)
{
  if (renderer) memset(renderer, 0, sizeof(*renderer));
}

void
mulib_imtui_renderer_free(mulib_imtui_renderer_t *renderer)
{
  if (!renderer) return;
  free(renderer->baseline);
  mulib_terminfo_free(&renderer->profile);
  mulib_imtui_renderer_init(renderer);
}

void
mulib_imtui_renderer_invalidate(mulib_imtui_renderer_t *renderer)
{
  if (renderer) renderer->valid = 0;
}

static int
mulib_imtui_render_same_profile_(const mulib_terminfo_t *a, const mulib_terminfo_t *b)
{
  if (a->ansi || b->ansi) return a->ansi && b->ansi;
  if (!a->data || !b->data) return !a->data && !b->data;
  return a->length == b->length && !memcmp(a->data, b->data, a->length);
}

static int
mulib_imtui_render_same_style_(mulib_term_style_t a, mulib_term_style_t b)
{
  return a.foreground == b.foreground && a.background == b.background
         && a.attributes == b.attributes;
}

/* Keep worst-case storage bounded even if a capability expands to a large
 * sequence for every cell. */
static int
mulib_imtui_render_bounded_(const mulib_strbuf_t *out)
{
  return out->length <= UINT32_C(67108864);
}

static int
mulib_imtui_render_emit_(const mulib_terminfo_t *info, const char *cap,
                         const int32_t params[9], mulib_strbuf_t *out)
{
  size_t before = out->length;
  return mulib_terminfo_emit(info, cap, params, out) && out->length > before
         && mulib_imtui_render_bounded_(out);
}

static int
mulib_imtui_render_style_(mulib_term_style_t style, const mulib_terminfo_t *info,
                          unsigned flags, mulib_strbuf_t *out)
{
  return mulib_termstyle_emit(style, info, flags, out) == MULIB_TERMSTYLE_OK
         && mulib_imtui_render_bounded_(out);
}

static int
mulib_imtui_render_cell_(mulib_term_cell_t cell, mulib_strbuf_t *out)
{
  unsigned char utf8[4];
  size_t n;
  if (!mulib_term_cell_valid(cell)) return 0;
  n = mulib_utf8_encode(cell.codepoint, utf8, sizeof(utf8));
  return n && mulib_strbuf_append_n(out, (const char *)utf8, n)
         && mulib_imtui_render_bounded_(out);
}

static int
mulib_imtui_render_plain_(const mulib_term_cell_t *cells, int w, int h,
                          mulib_strbuf_t *out)
{
  int x, y;
  for (y = 0; y < h; ++y)
    {
      for (x = 0; x < w; ++x)
        if (!mulib_imtui_render_cell_(*cells++, out)) return 0;
      if (!mulib_strbuf_putc(out, '\n') || !mulib_imtui_render_bounded_(out)) return 0;
    }
  return 1;
}

int
mulib_imtui_render(mulib_imtui_renderer_t *renderer, const mulib_imtui_t *ui,
                   const mulib_terminfo_t *info, unsigned flags, mulib_strbuf_t *out)
{
  const mulib_term_cell_t *cells;
  mulib_term_cell_t *allocated = NULL;
  mulib_imtui_rect_t bounds;
  mulib_terminfo_t new_profile;
  mulib_strbuf_t frame;
  size_t count, i = 0;
  int x, y, full, profile_changed, no_wrap = 0, can_move_styled, ok = 0;
  int began = 0, have_style = 0;
  mulib_term_style_t normal = mulib_term_style_default(), current = normal;
  if (!ui || !out || (flags & ~(unsigned)(MULIB_TERMSTYLE_TRUECOLOR | MULIB_TERMSTYLE_PLAIN)))
    return 0;
  bounds = mulib_imtui_bounds(ui);
  cells = mulib_imtui_cells(ui);
  if (bounds.w <= 0 || bounds.h <= 0 || !cells
      || (size_t)bounds.w > SIZE_MAX / sizeof(*cells) / (size_t)bounds.h)
    return 0;
  count = (size_t)bounds.w * (size_t)bounds.h;
  mulib_strbuf_init(&frame);
  mulib_terminfo_init(&new_profile);
  if (flags & MULIB_TERMSTYLE_PLAIN)
    {
      ok = mulib_imtui_render_plain_(cells, bounds.w, bounds.h, &frame)
           && mulib_strbuf_append_n(out, frame.data, frame.length);
      goto done;
    }
  if (!renderer || !info || ((flags & MULIB_TERMSTYLE_TRUECOLOR) && !info->ansi)) goto done;

  profile_changed = !mulib_imtui_render_same_profile_(&renderer->profile, info);
  full = !renderer->valid || renderer->width != bounds.w || renderer->height != bounds.h
         || renderer->flags != flags || profile_changed;
  /* Don't leave a terminal with immediate automatic margins scrolling after
   * a bottom-right write. On delayed-wrap terminals the final cursor restore
   * can cancel pending wrap without a mode change. */
  if ((full || !mulib_term_cell_equal(cells[count - 1], renderer->baseline[count - 1]))
      && mulib_terminfo_bool(info, "am") == 1 && mulib_terminfo_bool(info, "xenl") != 1)
    no_wrap = 1;
  can_move_styled = mulib_terminfo_bool(info, "msgr") == 1;

  for (y = 0; y < bounds.h; ++y)
    {
      int in_run = 0;
      for (x = 0; x < bounds.w; ++x, ++i)
        {
          const mulib_term_cell_t *cell = &cells[i];
          if (!full && mulib_term_cell_equal(*cell, renderer->baseline[i]))
            {
              in_run = 0;
              continue;
            }
          if (!began)
            {
              if (!mulib_imtui_render_emit_(info, "sc", NULL, &frame)
                  || (no_wrap && !mulib_imtui_render_emit_(info, "rmam", NULL, &frame)))
                goto done;
              began = 1;
            }
          if (!in_run)
            {
              int32_t pos[9] = {(int32_t)y, (int32_t)x};
              if (!can_move_styled && (!have_style || !mulib_imtui_render_same_style_(current, normal)))
                {
                  if (!mulib_imtui_render_style_(normal, info, flags, &frame)) goto done;
                  current = normal;
                  have_style = 1;
                }
              if (!mulib_imtui_render_emit_(info, "cup", pos, &frame)) goto done;
              in_run = 1;
            }
          if (!have_style || !mulib_imtui_render_same_style_(cell->style, current))
            {
              if (!mulib_imtui_render_style_(cell->style, info, flags, &frame)) goto done;
              current = cell->style;
              have_style = 1;
            }
          if (!mulib_imtui_render_cell_(*cell, &frame)) goto done;
        }
    }
  if (!began)
    {
      ok = 1;
      goto done;
    }
  if ((!mulib_imtui_render_same_style_(current, normal)
       && !mulib_imtui_render_style_(normal, info, flags, &frame))
      || (no_wrap && !mulib_imtui_render_emit_(info, "smam", NULL, &frame))
      || !mulib_imtui_render_emit_(info, "rc", NULL, &frame))
    goto done;
  if (count > renderer->capacity)
    {
      allocated = (mulib_term_cell_t *)malloc(count * sizeof(*cells));
      if (!allocated) goto done;
    }
  if (profile_changed && !mulib_terminfo_copy(&new_profile, info)) goto done;
  if (!mulib_strbuf_append_n(out, frame.data, frame.length)) goto done;

  if (allocated)
    {
      free(renderer->baseline);
      renderer->baseline = allocated;
      renderer->capacity = count;
      allocated = NULL;
    }
  if (profile_changed)
    {
      mulib_terminfo_free(&renderer->profile);
      renderer->profile = new_profile;
      mulib_terminfo_init(&new_profile);
    }
  memcpy(renderer->baseline, cells, count * sizeof(*cells));
  renderer->width = bounds.w;
  renderer->height = bounds.h;
  renderer->flags = flags;
  renderer->valid = 1;
  ok = 1;
done:
  free(allocated);
  mulib_terminfo_free(&new_profile);
  mulib_strbuf_free(&frame);
  return ok;
}

#ifdef __cplusplus
}
#endif
#endif
