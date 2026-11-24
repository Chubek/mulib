/** @file mulib-termfunc.h
 * @brief Semantic terminal operations backed by terminfo and explicit xterm modes.
 * Define MULIB_IMPL_TERMFUNC, MULIB_IMPL_TERMINFO and MULIB_IMPL_STRBUF once.
 */
#ifndef MULIB_TERMFUNC_H
#define MULIB_TERMFUNC_H
#include "mulib-terminfo.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  MULIB_TERMFUNC_MOVE, MULIB_TERMFUNC_HOME, MULIB_TERMFUNC_CLEAR,
  MULIB_TERMFUNC_ERASE_LINE, MULIB_TERMFUNC_ERASE_BELOW, MULIB_TERMFUNC_REGION,
  MULIB_TERMFUNC_CURSOR_SHOW, MULIB_TERMFUNC_CURSOR_HIDE,
  MULIB_TERMFUNC_SCREEN_ENTER, MULIB_TERMFUNC_SCREEN_LEAVE,
  MULIB_TERMFUNC_KEYPAD_ENTER, MULIB_TERMFUNC_KEYPAD_LEAVE,
  MULIB_TERMFUNC_PASTE_ON, MULIB_TERMFUNC_PASTE_OFF,
  MULIB_TERMFUNC_MOUSE_ON, MULIB_TERMFUNC_MOUSE_OFF,
  MULIB_TERMFUNC_RESET, MULIB_TERMFUNC_BELL
} mulib_termfunc_op_t;

enum {
  MULIB_TERMFUNC_XTERM_MODES = 1u,
  MULIB_TERMFUNC_OK = 1, MULIB_TERMFUNC_UNSUPPORTED = 0,
  MULIB_TERMFUNC_INVALID = -1, MULIB_TERMFUNC_MEMORY = -2
};

/* `info` is explicit; NULL does not guess a terminal. MOVE arguments are
 * zero-based column, row. REGION takes inclusive top, bottom rows. All other
 * operations take zero, zero. Xterm paste and SGR mouse require opt-in.
 * Append atomically; no I/O and no environment access. */
int mulib_termfunc_emit(const mulib_terminfo_t *info, unsigned flags,
                        mulib_termfunc_op_t op, int32_t first, int32_t second,
                        mulib_strbuf_t *out);
/* Resolve a case-sensitive semantic name such as "cursor-hide" or "move". */
int mulib_termfunc_lookup(const char *name, size_t length, mulib_termfunc_op_t *op);

#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_TERMFUNC) && !defined(MULIB_TERMFUNC_IMPLEMENTED)
#define MULIB_TERMFUNC_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

static const struct {
  const char *name, *capability, *xterm;
} mulib_termfunc_ops_[] = {
  { "move", "cup", NULL }, { "home", "home", NULL }, { "clear", "clear", NULL },
  { "erase-line", "el", NULL }, { "erase-below", "ed", NULL }, { "region", "csr", NULL },
  { "cursor-show", "cnorm", NULL }, { "cursor-hide", "civis", NULL },
  { "screen-enter", "smcup", NULL }, { "screen-leave", "rmcup", NULL },
  { "keypad-enter", "smkx", NULL }, { "keypad-leave", "rmkx", NULL },
  /* xterm extensions, separately profiled in .sot/term/termscript-profile.md. */
  { "paste-on", NULL, "\033[?2004h" }, { "paste-off", NULL, "\033[?2004l" },
  { "mouse-on", NULL, "\033[?1000h\033[?1006h" },
  { "mouse-off", NULL, "\033[?1006l\033[?1000l" },
  { "reset", "sgr0", NULL }, { "bell", "bel", NULL }
};

int
mulib_termfunc_lookup(const char *name, size_t n, mulib_termfunc_op_t *op)
{
  if (!name || !op) return 0;
  for (size_t i = 0; i < sizeof mulib_termfunc_ops_ / sizeof *mulib_termfunc_ops_; ++i) {
    const char *text = mulib_termfunc_ops_[i].name;
    if (strlen(text) == n && !memcmp(text, name, n)) {
      *op = (mulib_termfunc_op_t)i;
      return 1;
    }
  }
  return 0;
}

int
mulib_termfunc_emit(const mulib_terminfo_t *info, unsigned flags, mulib_termfunc_op_t op,
                    int32_t first, int32_t second, mulib_strbuf_t *out)
{
  int32_t args[9] = { 0 };
  if (!out || (flags & ~((unsigned)MULIB_TERMFUNC_XTERM_MODES))
      || (unsigned)op >= sizeof mulib_termfunc_ops_ / sizeof *mulib_termfunc_ops_)
    return MULIB_TERMFUNC_INVALID;
  if (op == MULIB_TERMFUNC_MOVE || op == MULIB_TERMFUNC_REGION) {
    if (first < 0 || second < 0 || first == INT32_MAX || second == INT32_MAX
        || (op == MULIB_TERMFUNC_REGION && first > second))
      return MULIB_TERMFUNC_INVALID;
    args[0] = op == MULIB_TERMFUNC_MOVE ? second : first;
    args[1] = op == MULIB_TERMFUNC_MOVE ? first : second;
  } else if (first || second) return MULIB_TERMFUNC_INVALID;
  const char *cap = mulib_termfunc_ops_[op].capability;
  if (!cap) {
    if (!(flags & MULIB_TERMFUNC_XTERM_MODES)) return MULIB_TERMFUNC_UNSUPPORTED;
    return mulib_strbuf_append(out, mulib_termfunc_ops_[op].xterm)
      ? MULIB_TERMFUNC_OK : MULIB_TERMFUNC_MEMORY;
  }
  if (!info || !mulib_terminfo_str(info, cap)) return MULIB_TERMFUNC_UNSUPPORTED;
  return mulib_terminfo_emit(info, cap, args, out) ? MULIB_TERMFUNC_OK : MULIB_TERMFUNC_UNSUPPORTED;
}

#ifdef __cplusplus
}
#endif
#endif
