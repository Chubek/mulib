#define MULIB_IMPL_IMMGTK
#define MULIB_IMPL_GLX
#define MULIB_IMPL_DRM
#define MULIB_IMPL_LINALG
#define MULIB_IMPL_MUNE
#define MULIB_IMPL_DYNLOAD
#define MULIB_IMPL_MIR
#define MULIB_IMPL_VMWEAVE
#define MULIB_IMPL_EGRAPH
#define MULIB_IMPL_REWRITE
#define MULIB_IMPL_DYNCALL
#define MULIB_IMPL_JIT
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_PARSER
#define MULIB_IMPL_PEG
#define MULIB_IMPL_AST
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_GLOB
#include "mulib-immgtk.h"
#include <assert.h>
#include <string.h>

static int labels;
static int
text (mulib_glx_t *g, float x, float y, const char *s, size_t n,
      uint32_t color, void *user)
{
  (void)x;
  (void)y;
  (void)color;
  assert (g && s && n && user == &labels);
  ++labels;
  return 1;
}
static void
run (mulib_immgtk_t *g, const char *s)
{
  mulib_mune_error_t e;
  mulib_mune_status_t rc = mulib_immgtk_run (g, s, strlen (s), &e);
  if (rc)
    fprintf (stderr, "GUI: %s at %zu\n", e.message, e.offset);
  assert (rc == MULIB_MUNE_OK);
}
static int
boolean (mulib_immgtk_t *g, const char *name)
{
  mulib_mune_value_t v;
  assert (mulib_mune_get_global (mulib_immgtk_state (g), name, &v)
              == MULIB_MUNE_OK
          && v.type == MULIB_MUNE_BOOLEAN);
  return v.as.boolean;
}
int
main (void)
{
  const char *button = "clicked=ui.button(1,'Hello',4,4,50,20)";
  mulib_mune_error_t error;
  mulib_glx_t *surface = mulib_glx_create (128, 128);
  mulib_immgtk_t *g = mulib_immgtk_create (text, &labels, &error);
  mulib_immgtk_input_t in = { 10, 10, 0, 0 };
  mulib_mune_value_t value;
  if (!g)
    fprintf (stderr, "init: %s\n", error.message);
  assert (g && surface);
  assert (!mulib_immgtk_create (NULL, NULL, &error)
          && error.code == MULIB_MUNE_INVALID);
  assert (!mulib_immgtk_end (g));
  assert (mulib_immgtk_run (g, button, strlen (button), &error)
          == MULIB_MUNE_RUNTIME);
  run (g, "function custom(id) return ui.button(id,'Custom',60,4,60,20) end");
  assert (mulib_immgtk_begin (g, surface, in));
  assert (!mulib_immgtk_begin (g, surface, in));
  run (g, button);
  assert (!boolean (g, "clicked") && mulib_immgtk_hot (g) == 1);
  assert (mulib_immgtk_end (g));
  in.down = 1;
  assert (mulib_immgtk_begin (g, surface, in));
  run (g, button);
  assert (mulib_immgtk_active (g) == 1 && !boolean (g, "clicked"));
  assert (mulib_immgtk_end (g));
  in.down = 0;
  assert (mulib_immgtk_begin (g, surface, in));
  run (g, button);
  assert (boolean (g, "clicked"));
  assert (mulib_immgtk_end (g) && mulib_immgtk_active (g) == 0
          && mulib_immgtk_focus (g) == 1);
  in.mouse_x = 100;
  in.activate = 1;
  assert (mulib_immgtk_begin (g, surface, in));
  run (g, button);
  assert (boolean (g, "clicked"));
  assert (mulib_immgtk_end (g));
  in.activate = 0;
  in.mouse_x = 10;
  in.down = 1;
  assert (mulib_immgtk_begin (g, surface, in));
  run (g, button);
  assert (mulib_immgtk_end (g));
  in.down = 0;
  in.mouse_x = 100;
  assert (mulib_immgtk_begin (g, surface, in));
  run (g, button);
  assert (!boolean (g, "clicked"));
  assert (mulib_immgtk_end (g));
  in.down = 1;
  in.mouse_x = 10;
  assert (mulib_immgtk_begin (g, surface, in));
  run (g, "checked=ui.checkbox(2,'Toggle',false,4,4,80,20)");
  assert (!boolean (g, "checked"));
  assert (mulib_immgtk_end (g));
  in.down = 0;
  assert (mulib_immgtk_begin (g, surface, in));
  run (g, "checked=ui.checkbox(2,'Toggle',checked,4,4,80,20)");
  assert (boolean (g, "checked"));
  assert (mulib_immgtk_end (g));
  in.down = 1;
  in.mouse_x = 50;
  assert (mulib_immgtk_begin (g, surface, in));
  run (g, "amount=ui.slider(3,0,0,1,0,0,100,20)");
  assert (mulib_mune_get_global (mulib_immgtk_state (g), "amount", &value)
              == MULIB_MUNE_OK
          && value.type == MULIB_MUNE_NUMBER && value.as.number == 0.5);
  assert (mulib_immgtk_end (g));
  in.mouse_x = 200;
  assert (mulib_immgtk_begin (g, surface, in));
  run (g, "amount=ui.slider(3,amount,0,1,0,0,100,20)");
  assert (mulib_mune_get_global (mulib_immgtk_state (g), "amount", &value)
          == MULIB_MUNE_OK);
  assert ((value.type == MULIB_MUNE_NUMBER && value.as.number == 1)
          || (value.type == MULIB_MUNE_INTEGER && value.as.integer == 1));
  assert (mulib_immgtk_end (g));
  assert (mulib_immgtk_begin (g, surface, in));
  run (g, button);
  assert (mulib_immgtk_run (g, button, strlen (button), &error)
          == MULIB_MUNE_RUNTIME);
  assert (mulib_immgtk_end (g));
  in.down = 0;
  assert (mulib_immgtk_begin (g, surface, in));
  run (g, "ui.layout(4,30,100,20,2); ui.button(4,'Layout'); custom(5)");
  assert (mulib_immgtk_end (g));
  assert (labels >= 10);
  mulib_immgtk_destroy (g);
  mulib_glx_destroy (surface);
  return 0;
}
