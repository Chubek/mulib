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
main (int argc, char **argv)
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
  /* Creation is widget-free; applications can use only host primitives. */
  run (g, "assert(ui==nil and package.loaded['immgtk']==nil); "
          "assert(type(imm_rect)=='function' and type(imm_text)=='function' "
          "and type(imm_pointer)=='function' and type(imm_interact)=='function')");
  assert (mulib_mune_make_string (mulib_immgtk_state (g),
                                  argc > 1 ? argv[1] : "library",
                                  strlen (argc > 1 ? argv[1] : "library"),
                                  &value) == MULIB_MUNE_OK);
  assert (mulib_mune_set_global (mulib_immgtk_state (g), "immgtk_library",
                                 value) == MULIB_MUNE_OK);
  run (g, "package.path=immgtk_library..'/?.lua;'..immgtk_library..'/?/init.lua'; "
          "local part=require('immgtk.button'); "
          "assert(type(part.button)=='function' and part.checkbox==nil and ui==nil); "
          "ui=require('immgtk'); assert(ui==part and require('immgtk')==ui); "
          "dofile(immgtk_library..'/../tests/immgtk-widgets.lua')");
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
  in.down = 0;
  in.mouse_x = -100;
  assert (mulib_immgtk_begin (g, surface, in));
  run (g, "immgtk_test_draw(); immgtk_test_invalid()");
  assert (mulib_immgtk_end (g));
  /* Verify actual pixels rather than only successful script execution. */
  assert (mulib_immgtk_begin (g, surface, in));
  run (g, "ui.panel(0,0,128,128); ui.progress(0.5,0,1,0,0,100,20); "
          "ui.image({0xffff0000,0xff00ff00,0xff0000ff,0xffffffff},"
          "2,2,0,20,20,20)");
  {
    const uint32_t *pixels = mulib_glx_pixels (surface);
    size_t stride = mulib_glx_pitch (surface) / sizeof (*pixels);
    assert (pixels[10 * stride + 10] == UINT32_C (0xff56b4e9));
    assert (pixels[10 * stride + 75] == UINT32_C (0xff343b48));
    assert (pixels[25 * stride + 5] == UINT32_C (0xffff0000));
    assert (pixels[25 * stride + 15] == UINT32_C (0xff00ff00));
    assert (pixels[35 * stride + 5] == UINT32_C (0xff0000ff));
    assert (pixels[35 * stride + 15] == UINT32_C (0xffffffff));
    assert (pixels[100 * stride + 100] == UINT32_C (0xff202630));
  }
  assert (mulib_immgtk_end (g));
  for (int i = 1; i <= 17; ++i)
    {
      char script[80];
      in.mouse_x = 75;
      in.mouse_y = 10;
      for (int down = 1; down >= 0; --down)
        {
          in.down = down;
          snprintf (script, sizeof script, "immgtk_test_interact(%d,%s)", i,
                    down ? "true" : "false");
          assert (mulib_immgtk_begin (g, surface, in));
          run (g, script);
          assert (mulib_immgtk_end (g));
        }
    }
  /* The host still rejects malformed primitives regardless of widget code. */
  in.mouse_x = NAN;
  assert (!mulib_immgtk_begin (g, surface, in));
  in.mouse_x = 0;
  assert (mulib_immgtk_begin (g, surface, in));
  run (g, "assert(not pcall(imm_rect,0,0,-1,1,0)); "
          "assert(not pcall(imm_text,'x',0,0,0.5)); "
          "assert(not pcall(imm_interact,0,0,0,1,1)); "
          "assert(not pcall(imm_interact,1.5,0,0,1,1)); "
          "assert(not pcall(imm_interact,9007199254740992,0,0,1,1)); "
          "assert(not pcall(imm_interact,1,0,0,0,1)); "
          "for i=1,256 do imm_interact(i,0,0,1,1) end; "
          "assert(not pcall(imm_interact,257,0,0,1,1))");
  assert (mulib_immgtk_end (g));
  mulib_immgtk_destroy (g);
  mulib_glx_destroy (surface);
  return 0;
}
