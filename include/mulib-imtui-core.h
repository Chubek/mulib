/**
 * @file mulib-imtui-core.h
 * @brief Immediate-mode terminal cells, clipping, IDs and input routing.
 *
 * No terminal I/O is performed. Define MULIB_IMPL_IMTUI_CORE and link the
 * VTERM, GRID and UTF8 implementations. TERMKEY supplies the event type;
 * its implementation is needed only by callers that decode terminal input.
 */
#ifndef MULIB_IMTUI_CORE_H
#define MULIB_IMTUI_CORE_H
#include "mulib-termkey.h"
#include "mulib-vterm.h"
#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Maximum depth of each clip and ID stack. */
#define MULIB_IMTUI_STACK_CAPACITY 32u
/** @brief Maximum enabled visible interactive items per frame. */
#define MULIB_IMTUI_MAX_ITEMS 256u
/** @brief Maximum framebuffer area in cells. */
#define MULIB_IMTUI_MAX_CELLS MULIB_VTERM_MAX_CELLS

  typedef struct mulib_imtui_rect
  {
    int x, y, w, h;
  } mulib_imtui_rect_t;
  typedef uint64_t mulib_imtui_id_t;

  enum mulib_imtui_flags
  {
    MULIB_IMTUI_HOVERED = 1u,
    MULIB_IMTUI_FOCUSED = 2u,
    MULIB_IMTUI_PRESSED = 4u,
    MULIB_IMTUI_ACTIVATED = 8u,
    MULIB_IMTUI_HELD = 16u
  };

  typedef struct mulib_imtui_item
  {
    mulib_imtui_id_t id;
    mulib_imtui_rect_t rect;
    size_t key_head, key_tail;
    unsigned pulses;
  } mulib_imtui_item_t;
  struct mulib_imtui_event_route;

  /** @brief Caller-owned context. Inspect members, but do not mutate or copy. */
  typedef struct mulib_imtui
  {
    int width, height;
    mulib_term_cell_t *framebuffer;
    mulib_imtui_rect_t clips[MULIB_IMTUI_STACK_CAPACITY];
    mulib_imtui_id_t scopes[MULIB_IMTUI_STACK_CAPACITY];
    size_t clip_depth, scope_depth;
    mulib_imtui_id_t focus, active, hovered, paste_focus;
    size_t mouse_x, mouse_y;
    int in_frame, mouse_known, mouse_down, pasting;
    mulib_imtui_item_t items[MULIB_IMTUI_MAX_ITEMS];
    mulib_imtui_item_t previous[MULIB_IMTUI_MAX_ITEMS];
    size_t item_count, previous_count;
    const mulib_termkey_event_t *events;
    struct mulib_imtui_event_route *routes;
    size_t route_capacity;
    size_t first_press, last_press, capture_press, paste_press;
  } mulib_imtui_t;

  /** @brief Initialize an uninitialized context; failure leaves it freeable. */
  int mulib_imtui_init (mulib_imtui_t *ui, int w, int h);
  /** @brief Release owned storage and zero the context; NULL is allowed. */
  void mulib_imtui_free (mulib_imtui_t *ui);
  /** @brief Resize between frames, atomically preserving the top-left cells.
   * Newly exposed cells are default blanks. A changed size cancels capture.
   * Dimensions must be positive and at most MULIB_IMTUI_MAX_CELLS in total. */
  int mulib_imtui_resize (mulib_imtui_t *ui, int w, int h);
  /** @brief Begin a frame and clear cells to default blanks. Returns 1/0.
   * Borrow an immutable event array until end(); NULL is allowed for count
   * zero. A failed begin does not alter the frame or consume input. Input is
   * processed in order. Tab/Shift-Tab traverse the last completed frame's
   * enabled visible IDs in submission order, wrapping. Tab is ignored if
   * that order is empty. */
  int mulib_imtui_begin (mulib_imtui_t *ui,
                         const mulib_termkey_event_t *events, size_t count);
  /** @brief Finish a frame; drop unclaimed events and clear focus/capture
   * when their owner was not submitted as enabled and visible. */
  void mulib_imtui_end (mulib_imtui_t *ui);
  mulib_imtui_rect_t mulib_imtui_bounds (const mulib_imtui_t *ui);
  /** @brief Borrow width*height row-major cells until a changed resize/free. */
  const mulib_term_cell_t *mulib_imtui_cells (const mulib_imtui_t *ui);
  /** @brief Write a validated cell inside the current clip during a frame.
   * Returns 1 if written, 0 for invalid input or a clipped coordinate. */
  int mulib_imtui_put (mulib_imtui_t *ui, int x, int y,
                       mulib_term_cell_t cell);
  /** @brief Return the current clip, or the full bounds outside a frame. */
  mulib_imtui_rect_t mulib_imtui_clip (const mulib_imtui_t *ui);
  /** @brief Intersect with the current clip; stacking an empty clip succeeds.
   * Rectangles are half-open; nonpositive dimensions are treated as empty.
   * A failed push leaves the stack unchanged. */
  int mulib_imtui_clip_push (mulib_imtui_t *ui, mulib_imtui_rect_t rect);
  void mulib_imtui_clip_pop (mulib_imtui_t *ui);
  /** @brief Hash length-delimited bytes in the current scope. Zero means
   * invalid; an empty label is legal. The root scope is available between
   * frames. IDs must be unique among interactive items in a frame. */
  mulib_imtui_id_t mulib_imtui_id (const mulib_imtui_t *ui, const char *label,
                                   size_t length);
  /** @brief Push a nonzero ID as a nested scope during a frame. Returns 1/0. */
  int mulib_imtui_id_push (mulib_imtui_t *ui, mulib_imtui_id_t scope);
  void mulib_imtui_id_pop (mulib_imtui_t *ui);
  /** @brief Register an item. Disabled, fully clipped, duplicate or excess
   * submissions return zero. First submitted wins overlapping hit tests.
   * Pointer press/release uses the last displayed geometry; when no enabled
   * geometry has been submitted yet, it uses current submission geometry.
   * A left press focuses and captures; a release inside activates. A press
   * on the background clears focus. Hover uses current geometry. HOVERED,
   * FOCUSED and HELD describe the resulting state; PRESSED and ACTIVATED
   * report at least one matching event in the batch. */
  unsigned mulib_imtui_interact (mulib_imtui_t *ui, mulib_imtui_id_t id,
                                 mulib_imtui_rect_t rect, int enabled);
  /** @brief Like interact(); when keyboard_activate is nonzero, consume
   * unmodified Enter/Space addressed to this item as activation pulses.
   * Pass zero for text input so take_key() can receive those keys. */
  unsigned mulib_imtui_interact_ex (mulib_imtui_t *ui, mulib_imtui_id_t id,
                                    mulib_imtui_rect_t rect, int enabled,
                                    int keyboard_activate);
  /** @brief After registering id, consume its next key/paste event, returning
   * 1/0. Keys follow focus at the time of the event, even if focus changes
   * later in the batch. Tab used for traversal is not returned. An opened
   * bracketed paste stays with its original focus until it ends. */
  int mulib_imtui_take_key (mulib_imtui_t *ui, mulib_imtui_id_t id,
                            mulib_termkey_event_t *event);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMTUI_CORE_H */

#if defined(MULIB_IMPL_IMTUI_CORE) && !defined(MULIB_IMTUI_CORE_IMPLEMENTED)
#define MULIB_IMTUI_CORE_IMPLEMENTED
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif

  struct mulib_imtui_event_route
  {
    size_t next, key_head, key_tail, release;
    int claimed;
  };

  static size_t
  mulib_imtui_cell_count_ (int w, int h)
  {
    if (w <= 0 || h <= 0 || (uintmax_t)w > MULIB_IMTUI_MAX_CELLS
        || (uintmax_t)h > MULIB_IMTUI_MAX_CELLS / (uintmax_t)w
        || (uintmax_t)h > SIZE_MAX / sizeof (mulib_term_cell_t) / (uintmax_t)w)
      return 0;
    return (size_t)w * (size_t)h;
  }

  static void
  mulib_imtui_blank_ (mulib_term_cell_t *cells, size_t count)
  {
    mulib_term_cell_t cell;
    cell.codepoint = ' ';
    cell.style = mulib_term_style_default ();
    for (size_t i = 0; i < count; ++i)
      cells[i] = cell;
  }

  static mulib_imtui_rect_t
  mulib_imtui_intersection_ (mulib_imtui_rect_t clip, mulib_imtui_rect_t r)
  {
    mulib_imtui_rect_t out = { 0, 0, 0, 0 };
    int x, y, right, bottom;
    if (clip.w <= 0 || clip.h <= 0 || r.w <= 0 || r.h <= 0)
      return out;
    x = r.x > clip.x ? r.x : clip.x;
    y = r.y > clip.y ? r.y : clip.y;
    right = r.x > INT_MAX - r.w ? INT_MAX : r.x + r.w;
    bottom = r.y > INT_MAX - r.h ? INT_MAX : r.y + r.h;
    if (right > clip.x + clip.w)
      right = clip.x + clip.w;
    if (bottom > clip.y + clip.h)
      bottom = clip.y + clip.h;
    if (right > x && bottom > y)
      {
        out.x = x;
        out.y = y;
        out.w = right - x;
        out.h = bottom - y;
      }
    return out;
  }

  static int
  mulib_imtui_contains_ (mulib_imtui_rect_t r, size_t x, size_t y)
  {
    return x >= (size_t)r.x && x - (size_t)r.x < (size_t)r.w
           && y >= (size_t)r.y && y - (size_t)r.y < (size_t)r.h;
  }

  static size_t
  mulib_imtui_find_ (const mulib_imtui_item_t *items, size_t n,
                     mulib_imtui_id_t id)
  {
    if (id)
      for (size_t i = 0; i < n; ++i)
        if (items[i].id == id)
          return i;
    return SIZE_MAX;
  }

  static size_t
  mulib_imtui_hit_ (const mulib_imtui_t *ui, size_t x, size_t y)
  {
    for (size_t i = 0; i < ui->previous_count; ++i)
      if (mulib_imtui_contains_ (ui->previous[i].rect, x, y))
        return i;
    return SIZE_MAX;
  }

  int
  mulib_imtui_init (mulib_imtui_t *ui, int w, int h)
  {
    size_t count = mulib_imtui_cell_count_ (w, h);
    if (!ui)
      return 0;
    memset (ui, 0, sizeof (*ui));
    if (!count)
      return 0;
    ui->framebuffer
        = (mulib_term_cell_t *)malloc (count * sizeof (*ui->framebuffer));
    if (!ui->framebuffer)
      return 0;
    ui->width = w;
    ui->height = h;
    mulib_imtui_blank_ (ui->framebuffer, count);
    return 1;
  }

  void
  mulib_imtui_free (mulib_imtui_t *ui)
  {
    if (ui)
      {
        free (ui->framebuffer);
        free (ui->routes);
        memset (ui, 0, sizeof (*ui));
      }
  }

  mulib_imtui_rect_t
  mulib_imtui_bounds (const mulib_imtui_t *ui)
  {
    mulib_imtui_rect_t r = { 0, 0, 0, 0 };
    if (ui && ui->framebuffer)
      {
        r.w = ui->width;
        r.h = ui->height;
      }
    return r;
  }

  const mulib_term_cell_t *
  mulib_imtui_cells (const mulib_imtui_t *ui)
  {
    return ui ? ui->framebuffer : NULL;
  }

  int
  mulib_imtui_resize (mulib_imtui_t *ui, int w, int h)
  {
    size_t count = mulib_imtui_cell_count_ (w, h), kept = 0;
    mulib_term_cell_t *cells;
    int copy_w, copy_h;
    if (!ui || !ui->framebuffer || ui->in_frame || !count)
      return 0;
    if (w == ui->width && h == ui->height)
      return 1;
    cells = (mulib_term_cell_t *)malloc (count * sizeof (*cells));
    if (!cells)
      return 0;
    mulib_imtui_blank_ (cells, count);
    copy_w = w < ui->width ? w : ui->width;
    copy_h = h < ui->height ? h : ui->height;
    for (int y = 0; y < copy_h; ++y)
      memcpy (cells + (size_t)y * (size_t)w,
              ui->framebuffer + (size_t)y * (size_t)ui->width,
              (size_t)copy_w * sizeof (*cells));
    free (ui->framebuffer);
    ui->framebuffer = cells;
    ui->width = w;
    ui->height = h;
    ui->active = ui->hovered = 0;
    /* The preserved image is still the input target, cropped to its bounds. */
    for (size_t i = 0; i < ui->previous_count; ++i)
      {
        mulib_imtui_item_t item = ui->previous[i];
        item.rect
            = mulib_imtui_intersection_ (mulib_imtui_bounds (ui), item.rect);
        if (item.rect.w)
          ui->previous[kept++] = item;
      }
    ui->previous_count = kept;
    if (mulib_imtui_find_ (ui->previous, kept, ui->focus) == SIZE_MAX)
      ui->focus = 0;
    if (mulib_imtui_find_ (ui->previous, kept, ui->paste_focus) == SIZE_MAX)
      ui->paste_focus = 0;
    return 1;
  }

  static void
  mulib_imtui_queue_key_ (mulib_imtui_t *ui, size_t *head, size_t *tail,
                          size_t i)
  {
    if (*head == SIZE_MAX)
      *head = i;
    else
      ui->routes[*tail].next = i;
    ui->routes[i].next = SIZE_MAX;
    *tail = i;
  }

  static void
  mulib_imtui_tab_ (mulib_imtui_t *ui, int reverse)
  {
    size_t n = ui->previous_count;
    size_t i = mulib_imtui_find_ (ui->previous, n, ui->focus);
    if (n)
      {
        if (reverse)
          i = i == SIZE_MAX || i == 0 ? n - 1 : i - 1;
        else
          i = i == SIZE_MAX || i + 1 == n ? 0 : i + 1;
        ui->focus = ui->previous[i].id;
      }
  }

  static void
  mulib_imtui_mouse_ (mulib_imtui_t *ui, size_t i)
  {
    const mulib_termkey_event_t *e = &ui->events[i];
    size_t hit;
    if (e->mouse_action != MULIB_TERMKEY_PRESS
        && e->mouse_action != MULIB_TERMKEY_RELEASE
        && e->mouse_action != MULIB_TERMKEY_MOVE
        && e->mouse_action != MULIB_TERMKEY_WHEEL)
      return;
    ui->mouse_known = 1;
    ui->mouse_x = e->x;
    ui->mouse_y = e->y;
    if (e->mouse_action == MULIB_TERMKEY_PRESS && e->mouse_button == 1
        && !ui->mouse_down)
      {
        ui->mouse_down = 1;
        hit = mulib_imtui_hit_ (ui, e->x, e->y);
        ui->focus = ui->active = hit == SIZE_MAX ? 0 : ui->previous[hit].id;
        if (hit != SIZE_MAX)
          ui->previous[hit].pulses |= MULIB_IMTUI_PRESSED;
        if (!ui->previous_count)
          {
            struct mulib_imtui_event_route *p = &ui->routes[i];
            /* Before the first visible layout, defer each gesture until
             * its press coordinates can be matched against submissions. */
            if (ui->last_press != SIZE_MAX)
              ui->routes[ui->last_press].next = i;
            else
              ui->first_press = i;
            ui->last_press = ui->capture_press = i;
            p->next = p->release = p->key_head = p->key_tail = SIZE_MAX;
            p->claimed = 0;
          }
      }
    else if (e->mouse_action == MULIB_TERMKEY_RELEASE
             && (e->mouse_button == 0 || e->mouse_button == 1)
             && ui->mouse_down)
      {
        hit = mulib_imtui_find_ (ui->previous, ui->previous_count, ui->active);
        if (hit != SIZE_MAX
            && mulib_imtui_contains_ (ui->previous[hit].rect, e->x, e->y))
          ui->previous[hit].pulses |= MULIB_IMTUI_ACTIVATED;
        if (ui->capture_press != SIZE_MAX)
          ui->routes[ui->capture_press].release = i;
        ui->capture_press = SIZE_MAX;
        ui->mouse_down = 0;
        ui->active = 0;
      }
  }

  static void
  mulib_imtui_route_key_ (mulib_imtui_t *ui, size_t i)
  {
    const mulib_termkey_event_t *e = &ui->events[i];
    mulib_imtui_id_t target = ui->focus;
    size_t press = ui->last_press, item;
    switch (e->type)
      {
      case MULIB_TERMKEY_KEY:
        if (e->key == MULIB_TERMKEY_NONE)
          return;
        if (e->key == MULIB_TERMKEY_TAB
            && !(e->modifiers & ~(unsigned)MULIB_TERMKEY_SHIFT))
          {
            mulib_imtui_tab_ (ui, e->modifiers != 0);
            return;
          }
        break;
      case MULIB_TERMKEY_PASTE_BEGIN:
        ui->pasting = 1;
        ui->paste_focus = target;
        ui->paste_press = press;
        break;
      case MULIB_TERMKEY_PASTE_TEXT:
      case MULIB_TERMKEY_PASTE_END:
        if (ui->pasting)
          {
            target = ui->paste_focus;
            press = ui->paste_press;
          }
        if (e->type == MULIB_TERMKEY_PASTE_END)
          {
            ui->pasting = 0;
            ui->paste_focus = 0;
            ui->paste_press = SIZE_MAX;
          }
        break;
      default:
        return;
      }
    item = mulib_imtui_find_ (ui->previous, ui->previous_count, target);
    if (item != SIZE_MAX)
      {
        mulib_imtui_item_t *p = &ui->previous[item];
        mulib_imtui_queue_key_ (ui, &p->key_head, &p->key_tail, i);
      }
    else if (press != SIZE_MAX)
      {
        struct mulib_imtui_event_route *p = &ui->routes[press];
        mulib_imtui_queue_key_ (ui, &p->key_head, &p->key_tail, i);
      }
  }

  int
  mulib_imtui_begin (mulib_imtui_t *ui, const mulib_termkey_event_t *events,
                     size_t count)
  {
    if (!ui || !ui->framebuffer || ui->in_frame || (!events && count)
        || count > SIZE_MAX / sizeof (*events)
        || count > SIZE_MAX / sizeof (*ui->routes))
      return 0;
    if (count > ui->route_capacity)
      {
        struct mulib_imtui_event_route *routes
            = (struct mulib_imtui_event_route *)realloc (
                ui->routes, count * sizeof (*routes));
        if (!routes)
          return 0;
        ui->routes = routes;
        ui->route_capacity = count;
      }
    ui->events = events;
    ui->first_press = ui->last_press = ui->capture_press = SIZE_MAX;
    ui->paste_press = SIZE_MAX;
    ui->item_count = ui->clip_depth = ui->scope_depth = 0;
    ui->hovered = 0;
    for (size_t i = 0; i < ui->previous_count; ++i)
      {
        ui->previous[i].pulses = 0;
        ui->previous[i].key_head = ui->previous[i].key_tail = SIZE_MAX;
      }
    for (size_t i = 0; i < count; ++i)
      if (events[i].type == MULIB_TERMKEY_MOUSE)
        mulib_imtui_mouse_ (ui, i);
      else
        mulib_imtui_route_key_ (ui, i);
    mulib_imtui_blank_ (ui->framebuffer,
                        (size_t)ui->width * (size_t)ui->height);
    ui->in_frame = 1;
    return 1;
  }

  void
  mulib_imtui_end (mulib_imtui_t *ui)
  {
    if (!ui || !ui->in_frame)
      return;
    if (mulib_imtui_find_ (ui->items, ui->item_count, ui->focus) == SIZE_MAX)
      ui->focus = 0;
    if (mulib_imtui_find_ (ui->items, ui->item_count, ui->active) == SIZE_MAX)
      ui->active = 0;
    if (mulib_imtui_find_ (ui->items, ui->item_count, ui->paste_focus)
        == SIZE_MAX)
      ui->paste_focus = 0;
    memcpy (ui->previous, ui->items, ui->item_count * sizeof (*ui->items));
    ui->previous_count = ui->item_count;
    ui->clip_depth = ui->scope_depth = 0;
    ui->events = NULL;
    ui->in_frame = 0;
  }

  mulib_imtui_rect_t
  mulib_imtui_clip (const mulib_imtui_t *ui)
  {
    return ui && ui->in_frame && ui->clip_depth ? ui->clips[ui->clip_depth - 1]
                                                : mulib_imtui_bounds (ui);
  }

  int
  mulib_imtui_put (mulib_imtui_t *ui, int x, int y, mulib_term_cell_t cell)
  {
    if (!ui || !ui->in_frame || x < 0 || y < 0 || x >= ui->width
        || y >= ui->height
        || !mulib_imtui_contains_ (mulib_imtui_clip (ui), (size_t)x, (size_t)y)
        || !mulib_term_cell_valid (cell))
      return 0;
    ui->framebuffer[(size_t)y * (size_t)ui->width + (size_t)x] = cell;
    return 1;
  }

  int
  mulib_imtui_clip_push (mulib_imtui_t *ui, mulib_imtui_rect_t r)
  {
    if (!ui || !ui->in_frame || ui->clip_depth == MULIB_IMTUI_STACK_CAPACITY)
      return 0;
    r = mulib_imtui_intersection_ (mulib_imtui_clip (ui), r);
    ui->clips[ui->clip_depth++] = r;
    return 1;
  }

  void
  mulib_imtui_clip_pop (mulib_imtui_t *ui)
  {
    if (ui && ui->in_frame && ui->clip_depth)
      --ui->clip_depth;
  }

  static uint64_t
  mulib_imtui_id_seed_ (const mulib_imtui_t *ui)
  {
    return ui->scope_depth ? ui->scopes[ui->scope_depth - 1]
                           : UINT64_C (14695981039346656037);
  }

  static uint64_t
  mulib_imtui_id_byte_ (uint64_t seed, unsigned byte)
  {
    return (seed ^ byte) * UINT64_C (1099511628211);
  }

  mulib_imtui_id_t
  mulib_imtui_id (const mulib_imtui_t *ui, const char *label, size_t length)
  {
    uint64_t hash;
    if (!ui || !ui->framebuffer || (!label && length))
      return 0;
    hash = mulib_imtui_id_byte_ (mulib_imtui_id_seed_ (ui), 0);
    for (size_t i = 0; i < length; ++i)
      hash = mulib_imtui_id_byte_ (hash, (unsigned char)label[i]);
    return hash ? hash : 1;
  }

  int
  mulib_imtui_id_push (mulib_imtui_t *ui, mulib_imtui_id_t scope)
  {
    uint64_t hash;
    if (!ui || !ui->in_frame || !scope
        || ui->scope_depth == MULIB_IMTUI_STACK_CAPACITY)
      return 0;
    hash = mulib_imtui_id_byte_ (mulib_imtui_id_seed_ (ui), 1);
    for (unsigned i = 0; i < 8; ++i, scope >>= 8)
      hash = mulib_imtui_id_byte_ (hash, (unsigned)(scope & 255u));
    ui->scopes[ui->scope_depth++] = hash;
    return 1;
  }

  void
  mulib_imtui_id_pop (mulib_imtui_t *ui)
  {
    if (ui && ui->in_frame && ui->scope_depth)
      --ui->scope_depth;
  }

  static void
  mulib_imtui_claim_presses_ (mulib_imtui_t *ui, mulib_imtui_item_t *item)
  {
    for (size_t i = ui->first_press; i != SIZE_MAX; i = ui->routes[i].next)
      {
        struct mulib_imtui_event_route *p = &ui->routes[i];
        const mulib_termkey_event_t *e = &ui->events[i];
        if (p->claimed || !mulib_imtui_contains_ (item->rect, e->x, e->y))
          continue;
        p->claimed = 1;
        item->pulses |= MULIB_IMTUI_PRESSED;
        if (i == ui->last_press)
          ui->focus = item->id;
        if (i == ui->capture_press)
          ui->active = item->id;
        if (i == ui->paste_press)
          ui->paste_focus = item->id;
        if (p->release != SIZE_MAX
            && mulib_imtui_contains_ (item->rect, ui->events[p->release].x,
                                      ui->events[p->release].y))
          item->pulses |= MULIB_IMTUI_ACTIVATED;
        /* Paste can straddle a later press even before there is a layout.
         * Merge by event index rather than concatenating gesture queues. */
        if (p->key_head != SIZE_MAX)
          {
            size_t *link = &item->key_head, k = p->key_head;
            while (k != SIZE_MAX)
              {
                size_t next = ui->routes[k].next;
                while (*link < k)
                  link = &ui->routes[*link].next;
                ui->routes[k].next = *link;
                *link = k;
                link = &ui->routes[k].next;
                k = next;
              }
          }
      }
  }

  unsigned
  mulib_imtui_interact_ex (mulib_imtui_t *ui, mulib_imtui_id_t id,
                           mulib_imtui_rect_t rect, int enabled,
                           int keyboard_activate)
  {
    mulib_imtui_item_t *item;
    size_t old;
    unsigned flags;
    if (!ui || !ui->in_frame || !id || !enabled
        || ui->item_count == MULIB_IMTUI_MAX_ITEMS
        || mulib_imtui_find_ (ui->items, ui->item_count, id) != SIZE_MAX)
      return 0;
    rect = mulib_imtui_intersection_ (mulib_imtui_clip (ui), rect);
    if (!rect.w)
      return 0;
    item = &ui->items[ui->item_count++];
    item->id = id;
    item->rect = rect;
    item->pulses = 0;
    item->key_head = item->key_tail = SIZE_MAX;
    old = mulib_imtui_find_ (ui->previous, ui->previous_count, id);
    if (old != SIZE_MAX)
      {
        item->pulses = ui->previous[old].pulses;
        item->key_head = ui->previous[old].key_head;
      }
    else if (!ui->previous_count)
      mulib_imtui_claim_presses_ (ui, item);
    flags = item->pulses;
    if (!ui->hovered && ui->mouse_known
        && mulib_imtui_contains_ (rect, ui->mouse_x, ui->mouse_y))
      {
        ui->hovered = id;
        flags |= MULIB_IMTUI_HOVERED;
      }
    if (ui->focus == id)
      flags |= MULIB_IMTUI_FOCUSED;
    if (ui->active == id)
      flags |= MULIB_IMTUI_HELD;
    if (keyboard_activate)
      {
        size_t *link = &item->key_head;
        while (*link != SIZE_MAX)
          {
            size_t i = *link;
            const mulib_termkey_event_t *e = &ui->events[i];
            if (e->type == MULIB_TERMKEY_KEY && !e->modifiers
                && (e->key == MULIB_TERMKEY_ENTER
                    || (e->key == MULIB_TERMKEY_CHAR && e->codepoint == ' ')))
              {
                flags |= MULIB_IMTUI_ACTIVATED;
                *link = ui->routes[i].next;
              }
            else
              link = &ui->routes[i].next;
          }
      }
    return flags;
  }

  unsigned
  mulib_imtui_interact (mulib_imtui_t *ui, mulib_imtui_id_t id,
                        mulib_imtui_rect_t rect, int enabled)
  {
    return mulib_imtui_interact_ex (ui, id, rect, enabled, 1);
  }

  int
  mulib_imtui_take_key (mulib_imtui_t *ui, mulib_imtui_id_t id,
                        mulib_termkey_event_t *event)
  {
    size_t item, i;
    if (!ui || !ui->in_frame || !event)
      return 0;
    item = mulib_imtui_find_ (ui->items, ui->item_count, id);
    if (item == SIZE_MAX || ui->items[item].key_head == SIZE_MAX)
      return 0;
    i = ui->items[item].key_head;
    *event = ui->events[i];
    ui->items[item].key_head = ui->routes[i].next;
    return 1;
  }

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMTUI_CORE_IMPLEMENTED */
