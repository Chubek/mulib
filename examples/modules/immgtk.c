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
#include <stdio.h>
#include <string.h>
/* Supply a font renderer here in an interactive application. This headless
 * example records label requests while the widgets draw to a GLX surface. */
static int
label (mulib_glx_t *surface, float x, float y, const char *text, size_t n,
       uint32_t color, void *user)
{
  (void)surface;
  (void)color;
  (void)user;
  printf ("label at %.0f,%.0f: ", (double)x, (double)y);
  return fwrite (text, 1, n, stdout) == n && putchar ('\n') != EOF;
}
int
main (void)
{
  const char *script = "ui.panel(0,0,160,100); ui.layout(8,8,144,24); if "
                       "ui.button(1,'Hello') then clicks=(clicks or 0)+1 end";
  mulib_immgtk_t *gui = mulib_immgtk_create (label, NULL, NULL);
  mulib_glx_t *surface = mulib_glx_create (160, 100);
  mulib_immgtk_input_t input = { 16, 16, 1, 0 };
  mulib_mune_value_t clicks;
  assert (gui && surface);
  for (int frame = 0; frame < 2; ++frame)
    {
      input.down = !frame;
      assert (mulib_immgtk_begin (gui, surface, input));
      assert (mulib_immgtk_run (gui, script, strlen (script), NULL)
              == MULIB_MUNE_OK);
      assert (mulib_immgtk_end (gui));
    }
  assert (mulib_mune_get_global (mulib_immgtk_state (gui), "clicks", &clicks)
          == MULIB_MUNE_OK);
  assert (clicks.type == MULIB_MUNE_INTEGER && clicks.as.integer == 1);
  mulib_immgtk_destroy (gui);
  mulib_glx_destroy (surface);
  return 0;
}
