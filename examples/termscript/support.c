/* Run semantic terminal operations inside MuLIB's virtual terminal. */
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_TERMINFO
#define MULIB_IMPL_TERMSTYLE
#define MULIB_IMPL_TERMFUNC
#define MULIB_IMPL_TERMUTILS
#define MULIB_IMPL_TERMSCRIPT_TERMINAL
#define MULIB_IMPL_TERMSCRIPT_STATE
#define MULIB_IMPL_GRID
#define MULIB_IMPL_VTERM
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_CSS
#define MULIB_IMPL_XML
#include "mulib-termscript-terminal.h"
#include "mulib-termscript-state.h"
#include <stdlib.h>

int
main(void)
{
  const mulib_termscript_transition_t transitions[] = { { 0, 1, 1 } };
  mulib_termscript_state_t machine;
  mulib_terminfo_t info;
  mulib_vterm_t screen;
  mulib_termscript_terminal_t terminal;
  if (!mulib_termscript_state_init(&machine, 0, transitions, 1)
      || !mulib_vterm_init(&screen, 8, 3)) return EXIT_FAILURE;
  mulib_terminfo_init(&info);
  mulib_terminfo_ansi(&info);
  mulib_termscript_terminal_init(&terminal, &info, 0, -1, mulib_termscript_terminal_vterm, &screen);
  int ok = mulib_termscript_state_send(&machine, 1, NULL, NULL) == 1
    && mulib_termscript_terminal_function(&terminal, MULIB_TERMFUNC_MOVE, 2, 1)
    && mulib_termscript_terminal_text(&terminal, "ready", 5)
    && mulib_vterm_cell(&screen, 2, 1)->codepoint == 'r';
  mulib_vterm_free(&screen);
  mulib_terminfo_free(&info);
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
