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
#undef NDEBUG
#include <assert.h>
#include <string.h>

static int
append(void *user, const char *bytes, size_t n)
{
  return mulib_strbuf_append_n(user, bytes, n);
}

static void
terminal(void)
{
  mulib_terminfo_t info;
  mulib_termscript_terminal_t t;
  mulib_strbuf_t buf = { NULL, 0, 0 };
  mulib_vterm_t screen;
  mulib_termfunc_op_t op = MULIB_TERMFUNC_HOME;
  unsigned cols = 1, rows = 2;
  mulib_terminfo_init(&info);
  mulib_terminfo_ansi(&info);
  assert(mulib_termfunc_lookup("move", 4, &op) && op == MULIB_TERMFUNC_MOVE);
  assert(!mulib_termfunc_lookup("nonesuch", 8, &op) && op == MULIB_TERMFUNC_MOVE);
  assert(mulib_termfunc_emit(&info, 0, MULIB_TERMFUNC_MOVE, 3, 1, &buf) == MULIB_TERMFUNC_OK);
  assert(!strcmp(buf.data, "\033[2;4H"));
  size_t length = buf.length;
  assert(mulib_termfunc_emit(&info, 0, MULIB_TERMFUNC_REGION, 3, 1, &buf) == MULIB_TERMFUNC_INVALID);
  assert(mulib_termfunc_emit(&info, 0, MULIB_TERMFUNC_PASTE_ON, 0, 0, &buf) == MULIB_TERMFUNC_UNSUPPORTED);
  assert(mulib_termfunc_emit(&info, 0, MULIB_TERMFUNC_MOVE, INT32_MAX, 0, &buf) == MULIB_TERMFUNC_INVALID);
  assert(buf.length == length && !strcmp(buf.data, "\033[2;4H"));
  mulib_strbuf_free(&buf);

  mulib_termscript_terminal_init(&t, &info, MULIB_TERMFUNC_XTERM_MODES, -1, append, &buf);
  assert(mulib_termscript_terminal_capability(&t, "kf11", 4, NULL));
  assert(mulib_termscript_terminal_function(&t, MULIB_TERMFUNC_PASTE_ON, 0, 0));
  assert(!strcmp(buf.data, "\033[23~\033[?2004h"));
  assert(!mulib_termscript_terminal_capability(&t, "kf11\0bad", 8, NULL));
  assert(mulib_termscript_terminal_raw(&t) == MULIB_TERMUTILS_ENOTSUP);
  assert(mulib_termscript_terminal_size(&t, &cols, &rows) == MULIB_TERMUTILS_ENOTSUP);
  assert(cols == 1 && rows == 2);
  assert(mulib_termscript_terminal_restore(&t) == MULIB_TERMUTILS_OK);
  mulib_strbuf_free(&buf);

  assert(mulib_vterm_init(&screen, 20, 4));
  mulib_termscript_terminal_init(&t, &info, 0, -1, mulib_termscript_terminal_vterm, &screen);
  assert(mulib_termscript_terminal_function(&t, MULIB_TERMFUNC_MOVE, 3, 1));
  assert(mulib_termscript_terminal_text(&t, "X\033[2J", 5));
  assert(mulib_vterm_cell(&screen, 3, 1)->codepoint == 'X');
  assert(mulib_vterm_cell(&screen, 4, 1)->codepoint == 0xfffd);
  assert(mulib_vterm_cell(&screen, 7, 1)->codepoint == 'J');
  assert(!mulib_termscript_terminal_text(&t, "\xc3", 1));
  mulib_vterm_free(&screen);
  mulib_terminfo_free(&info);
}

static int
reject(void *user, const mulib_termscript_transition_t *t)
{
  mulib_termscript_state_t *s = user;
  assert(s->state == t->source);
  assert(mulib_termscript_state_send(s, t->event, NULL, NULL) == -1);
  return 0;
}

static void
states(void)
{
  const mulib_termscript_transition_t table[] = {
    { INT64_MIN, 1, 42 }, { INT64_MIN, 2, INT64_MAX }, { 42, 1, INT64_MIN }
  };
  const mulib_termscript_transition_t duplicate[] = { { 0, 0, 1 }, { 0, 0, 2 } };
  mulib_termscript_transition_t found;
  mulib_termscript_state_t s;
  assert(mulib_termscript_state_init(&s, INT64_MIN, table, 3));
  assert(!mulib_termscript_state_init(&s, 0, duplicate, 2) && s.state == INT64_MIN);
  assert(mulib_termscript_state_find(&s, 1, &found) && found.target == 42);
  assert(!mulib_termscript_state_send(&s, 1, reject, &s) && s.state == INT64_MIN);
  assert(mulib_termscript_state_send(&s, 1, NULL, NULL) == 1 && s.state == 42);
  assert(mulib_termscript_state_send(&s, 1, NULL, NULL) == 1 && s.state == INT64_MIN);
  assert(mulib_termscript_state_send(&s, 2, NULL, NULL) == 1 && s.state == INT64_MAX);
  assert(!mulib_termscript_state_send(&s, 1, NULL, NULL) && s.state == INT64_MAX);
}

int
main(void)
{
  terminal();
  states();
  return 0;
}
