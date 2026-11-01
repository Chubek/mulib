/**
 * @file mulib-immgtk.h
 * @brief Immediate-mode GUI with Mune-written widgets and GLX drawing
 * primitives. Define MULIB_IMPL_IMMGTK and the transitive MUNE/GLX switches in
 * the manual. A context owns its Lua state. Surface, font callback and
 * callback data are borrowed.
 */
#ifndef MULIB_IMMGTK_H
#define MULIB_IMMGTK_H
#include "mulib-glx.h"
#include "mulib-mune.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_immgtk mulib_immgtk_t;
  /** @brief One frame's pointer state and a keyboard activation pulse for the
   * focused widget. */
  typedef struct mulib_immgtk_input
  {
    float mouse_x, mouse_y;
    int down, activate;
  } mulib_immgtk_input_t;
  /** @brief Draw borrowed UTF-8 label bytes. Return 1 on success. Use the
   * surface's scissor. */
  typedef int (*mulib_immgtk_text_fn) (mulib_glx_t *surface, float x, float y,
                                       const char *text, size_t length,
                                       uint32_t color, void *user);
  /** @brief Create a Mune state, bind primitives and load Lua widget
   * definitions. A text renderer is required; NULL on error with a static
   * diagnostic.
   */
  mulib_immgtk_t *mulib_immgtk_create (mulib_immgtk_text_fn text, void *user,
                                       mulib_mune_error_t *error);
  /** @brief Destroy owned state/context; NULL safe. */
  void mulib_immgtk_destroy (mulib_immgtk_t *gui);
  /** @brief Borrow state for application variables/native bindings; never
   * destroy it directly. */
  mulib_mune_state_t *mulib_immgtk_state (mulib_immgtk_t *gui);
  /** @brief Begin a frame; reject nested frames or nonfinite input. Returns
   * 1/0. Submit every visible widget once with a stable positive integer ID
   * <=2^53-1. The surface must outlive end(). At most 256 interactive widgets
   * per frame.
   */
  int mulib_immgtk_begin (mulib_immgtk_t *gui, mulib_glx_t *surface,
                          mulib_immgtk_input_t input);
  /** @brief Execute Mune code, retaining globals and releasing returned
   * values. Definitions can be loaded outside frames; drawing/interaction
   * require begin(). Lua/drawing side effects before an error remain
   * committed. Runtime error text is borrowed until the state's next
   * execution; follows mulib_mune_run_source.
   */
  mulib_mune_status_t mulib_immgtk_run (mulib_immgtk_t *gui,
                                        const char *source, size_t length,
                                        mulib_mune_error_t *error);
  /** @brief Finish a frame and release capture on pointer-up or a missing
   * widget. */
  int mulib_immgtk_end (mulib_immgtk_t *gui);
  /** @brief Query pointer-hot, captured, or keyboard-focused ID; zero means
   * none. */
  uint64_t mulib_immgtk_hot (const mulib_immgtk_t *gui);
  uint64_t mulib_immgtk_active (const mulib_immgtk_t *gui);
  uint64_t mulib_immgtk_focus (const mulib_immgtk_t *gui);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_IMMGTK) && !defined(MULIB_IMMGTK_IMPLEMENTED)
#define MULIB_IMMGTK_IMPLEMENTED
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  struct mulib_immgtk
  {
    mulib_mune_state_t *state;
    mulib_glx_t *surface;
    mulib_immgtk_text_fn text;
    void *user;
    mulib_immgtk_input_t input;
    int frame, previous, pressed, released;
    uint64_t hot, active, focus, ids[256];
    size_t count;
  };
  static int
  mulib_immgtk_number_ (mulib_mune_value_t v, double *out)
  {
    if (v.type == MULIB_MUNE_INTEGER)
      *out = (double)v.as.integer;
    else if (v.type == MULIB_MUNE_NUMBER)
      *out = v.as.number;
    else
      return 0;
    return isfinite (*out);
  }
  static int
  mulib_immgtk_numbers_ (const mulib_mune_value_t *a, size_t n, double *out)
  {
    for (size_t i = 0; i < n; ++i)
      if (!mulib_immgtk_number_ (a[i], out + i))
        return 0;
    return 1;
  }
  static int
  mulib_immgtk_geometry_ (double x, double y, double w, double h)
  {
    return fabs (x) <= 1000000 && fabs (y) <= 1000000 && w >= 0 && h >= 0
           && w <= 1000000 && h <= 1000000;
  }
  static int
  mulib_immgtk_boolean_ (mulib_mune_result_t *out, int b)
  {
    mulib_mune_value_t v;
    memset (&v, 0, sizeof v);
    v.type = MULIB_MUNE_BOOLEAN;
    v.as.boolean = b;
    return mulib_mune_result_push (out, v);
  }
  static mulib_mune_status_t
  mulib_immgtk_rect_ (mulib_mune_state_t *s, const mulib_mune_value_t *a,
                      size_t count, mulib_mune_result_t *out, void *user)
  {
    mulib_immgtk_t *g = (mulib_immgtk_t *)user;
    double v[5];
    (void)s;
    (void)out;
    if (!g->frame || count != 5 || !mulib_immgtk_numbers_ (a, 5, v)
        || !mulib_immgtk_geometry_ (v[0], v[1], v[2], v[3]) || v[4] < 0
        || v[4] > UINT32_MAX || v[4] != floor (v[4]))
      return MULIB_MUNE_RUNTIME;
    mulib_glx_fill_rect (g->surface, (int)floor (v[0]), (int)floor (v[1]),
                         (int)ceil (v[2]), (int)ceil (v[3]), (uint32_t)v[4]);
    return MULIB_MUNE_OK;
  }
  static mulib_mune_status_t
  mulib_immgtk_text_ (mulib_mune_state_t *s, const mulib_mune_value_t *a,
                      size_t count, mulib_mune_result_t *out, void *user)
  {
    mulib_immgtk_t *g = (mulib_immgtk_t *)user;
    double v[3];
    const char *text;
    size_t length;
    (void)s;
    (void)out;
    if (!g->frame || count != 4 || !mulib_immgtk_numbers_ (a + 1, 3, v)
        || fabs (v[0]) > 1000000 || fabs (v[1]) > 1000000 || v[2] < 0
        || v[2] > UINT32_MAX || v[2] != floor (v[2]))
      return MULIB_MUNE_RUNTIME;
    text = mulib_mune_value_string (a[0], &length);
    if (!text
        || !g->text (g->surface, (float)v[0], (float)v[1], text, length,
                     (uint32_t)v[2], g->user))
      return MULIB_MUNE_RUNTIME;
    return MULIB_MUNE_OK;
  }
  static mulib_mune_status_t
  mulib_immgtk_pointer_ (mulib_mune_state_t *s, const mulib_mune_value_t *a,
                         size_t count, mulib_mune_result_t *out, void *user)
  {
    mulib_immgtk_t *g = (mulib_immgtk_t *)user;
    mulib_mune_value_t v;
    (void)s;
    (void)a;
    if (!g->frame || count)
      return MULIB_MUNE_RUNTIME;
    memset (&v, 0, sizeof v);
    v.type = MULIB_MUNE_NUMBER;
    v.as.number = g->input.mouse_x;
    if (!mulib_mune_result_push (out, v))
      return MULIB_MUNE_MEMORY;
    v.as.number = g->input.mouse_y;
    return mulib_mune_result_push (out, v) ? MULIB_MUNE_OK : MULIB_MUNE_MEMORY;
  }
  static mulib_mune_status_t
  mulib_immgtk_interact_ (mulib_mune_state_t *s, const mulib_mune_value_t *a,
                          size_t count, mulib_mune_result_t *out, void *user)
  {
    mulib_immgtk_t *g = (mulib_immgtk_t *)user;
    double v[5];
    uint64_t id;
    int hover, held, clicked;
    (void)s;
    if (!g->frame || count != 5 || !mulib_immgtk_numbers_ (a, 5, v) || v[0] < 1
        || v[0] > 9007199254740991.0 || v[0] != floor (v[0])
        || !mulib_immgtk_geometry_ (v[1], v[2], v[3], v[4]) || v[3] == 0
        || v[4] == 0 || g->count == 256)
      return MULIB_MUNE_RUNTIME;
    id = (uint64_t)v[0];
    for (size_t i = 0; i < g->count; ++i)
      if (g->ids[i] == id)
        return MULIB_MUNE_RUNTIME;
    g->ids[g->count++] = id;
    hover = g->input.mouse_x >= v[1] && g->input.mouse_x < v[1] + v[3]
            && g->input.mouse_y >= v[2] && g->input.mouse_y < v[2] + v[4]
            && (!g->hot || g->hot == id);
    if (hover)
      g->hot = id;
    if (hover && g->pressed && !g->active)
      {
        g->active = id;
        g->focus = id;
      }
    held = g->active == id && g->input.down;
    clicked = (g->active == id && g->released && hover)
              || (g->focus == id && g->input.activate);
    if (!mulib_immgtk_boolean_ (out, hover)
        || !mulib_immgtk_boolean_ (out, held)
        || !mulib_immgtk_boolean_ (out, clicked)
        || !mulib_immgtk_boolean_ (out, g->focus == id))
      return MULIB_MUNE_MEMORY;
    return MULIB_MUNE_OK;
  }
  /* Widgets intentionally live in Mune, using only drawing/input host
   * primitives. */
  static const char mulib_immgtk_widgets_[]
      = "ui = { normal=0xff343b48, hover=0xff46556a, active=0xff2465a8,\n"
        "       text=0xffeeeeee, background=0xff202630, accent=0xff56b4e9 }\n"
        "function ui.layout(x,y,w,row,gap)\n"
        "  ui.lx=x; ui.ly=y; ui.lw=w; ui.lr=row or 24; ui.lg=gap or 4\n"
        "end\n"
        "function ui.next()\n"
        "  assert(ui.lx, 'call ui.layout first')\n"
        "  local y=ui.ly; ui.ly=y+ui.lr+ui.lg; return ui.lx,y,ui.lw,ui.lr\n"
        "end\n"
        "function ui.panel(x,y,w,h) imm_rect(x,y,w,h,ui.background) end\n"
        "function ui.label(text,x,y) imm_text(text,x,y,ui.text) end\n"
        "function ui.button(id,text,x,y,w,h)\n"
        "  if x==nil then x,y,w,h=ui.next() end\n"
        "  local hot,held,clicked=imm_interact(id,x,y,w,h)\n"
        "  imm_rect(x,y,w,h,held and ui.active or (hot and ui.hover or "
        "ui.normal))\n"
        "  imm_text(text,x+4,y+4,ui.text); return clicked\n"
        "end\n"
        "function ui.checkbox(id,text,value,x,y,w,h)\n"
        "  if x==nil then x,y,w,h=ui.next() end\n"
        "  assert(w>=h and h>=12, 'checkbox dimensions')\n"
        "  local hot,held,clicked=imm_interact(id,x,y,w,h)\n"
        "  if clicked then value=not value end\n"
        "  imm_rect(x,y,h,h,held and ui.active or (hot and ui.hover or "
        "ui.normal))\n"
        "  if value then imm_rect(x+4,y+4,h-8,h-8,ui.accent) end\n"
        "  imm_text(text,x+h+4,y+4,ui.text); return value,clicked\n"
        "end\n"
        "function ui.slider(id,value,lo,hi,x,y,w,h)\n"
        "  if x==nil then x,y,w,h=ui.next() end\n"
        "  assert(hi>lo and w>=8 and h>=8, 'slider range or dimensions')\n"
        "  local hot,held=imm_interact(id,x,y,w,h); local old=value\n"
        "  if held then local mx=imm_pointer(); value=lo+(mx-x)/w*(hi-lo) "
        "end\n"
        "  if value<lo then value=lo elseif value>hi then value=hi end\n"
        "  imm_rect(x,y,w,h,hot and ui.hover or ui.normal)\n"
        "  imm_rect(x,y,w*(value-lo)/(hi-lo),h,ui.accent)\n"
        "  return value,value~=old\n"
        "end\n";
  mulib_immgtk_t *
  mulib_immgtk_create (mulib_immgtk_text_fn text, void *user,
                       mulib_mune_error_t *error)
  {
    mulib_immgtk_t *g = NULL;
    mulib_mune_error_t e = { MULIB_MUNE_OK, 0, NULL };
    mulib_mune_status_t rc;
    if (!text)
      {
        e.code = MULIB_MUNE_INVALID;
        e.message = "text renderer required";
        goto fail;
      }
    g = (mulib_immgtk_t *)calloc (1, sizeof (*g));
    if (!g)
      goto memory;
    g->text = text;
    g->user = user;
    g->state = mulib_mune_state_create (NULL);
    if (!g->state)
      goto memory;
    if (mulib_mune_bind (g->state, "imm_rect", mulib_immgtk_rect_, g)
            != MULIB_MUNE_OK
        || mulib_mune_bind (g->state, "imm_text", mulib_immgtk_text_, g)
               != MULIB_MUNE_OK
        || mulib_mune_bind (g->state, "imm_pointer", mulib_immgtk_pointer_, g)
               != MULIB_MUNE_OK
        || mulib_mune_bind (g->state, "imm_interact", mulib_immgtk_interact_,
                            g)
               != MULIB_MUNE_OK)
      goto memory;
    rc = mulib_immgtk_run (g, mulib_immgtk_widgets_,
                           sizeof (mulib_immgtk_widgets_) - 1, &e);
    if (rc != MULIB_MUNE_OK)
      {
        e.message = "widget library initialization failed";
        goto fail;
      }
    if (error)
      *error = e;
    return g;
  memory:
    e.code = MULIB_MUNE_MEMORY;
    e.message = "allocation failed";
  fail:
    mulib_immgtk_destroy (g);
    if (error)
      *error = e;
    return NULL;
  }
  void
  mulib_immgtk_destroy (mulib_immgtk_t *g)
  {
    if (g)
      {
        mulib_mune_state_destroy (g->state);
        free (g);
      }
  }
  mulib_mune_state_t *
  mulib_immgtk_state (mulib_immgtk_t *g)
  {
    return g ? g->state : NULL;
  }
  int
  mulib_immgtk_begin (mulib_immgtk_t *g, mulib_glx_t *surface,
                      mulib_immgtk_input_t input)
  {
    if (!g || g->frame || !surface || !isfinite (input.mouse_x)
        || !isfinite (input.mouse_y) || (input.down != 0 && input.down != 1)
        || (input.activate != 0 && input.activate != 1))
      return 0;
    g->input = input;
    g->surface = surface;
    g->frame = 1;
    g->count = 0;
    g->hot = 0;
    g->pressed = input.down && !g->previous;
    g->released = !input.down && g->previous;
    return 1;
  }
  mulib_mune_status_t
  mulib_immgtk_run (mulib_immgtk_t *g, const char *source, size_t length,
                    mulib_mune_error_t *error)
  {
    mulib_mune_result_t *result = NULL;
    mulib_mune_status_t rc;
    if (!g)
      {
        if (error)
          {
            error->code = MULIB_MUNE_INVALID;
            error->offset = 0;
            error->message = "NULL GUI";
          }
        return MULIB_MUNE_INVALID;
      }
    rc = mulib_mune_run_source (g->state, source, length, &result, error);
    mulib_mune_result_destroy (result);
    return rc;
  }
  int
  mulib_immgtk_end (mulib_immgtk_t *g)
  {
    int active_seen = 0, focus_seen = 0;
    if (!g || !g->frame)
      return 0;
    for (size_t i = 0; i < g->count; ++i)
      {
        if (g->ids[i] == g->active)
          active_seen = 1;
        if (g->ids[i] == g->focus)
          focus_seen = 1;
      }
    if (!g->input.down || !active_seen)
      g->active = 0;
    if (!focus_seen)
      g->focus = 0;
    g->previous = g->input.down;
    g->frame = 0;
    g->surface = NULL;
    return 1;
  }
  uint64_t
  mulib_immgtk_hot (const mulib_immgtk_t *g)
  {
    return g ? g->hot : 0;
  }
  uint64_t
  mulib_immgtk_active (const mulib_immgtk_t *g)
  {
    return g ? g->active : 0;
  }
  uint64_t
  mulib_immgtk_focus (const mulib_immgtk_t *g)
  {
    return g ? g->focus : 0;
  }
#ifdef __cplusplus
}
#endif
#endif
