/** @file mulib-termscript-terminal.h
 * @brief Explicit output, capabilities, VTerm and POSIX termios for Termscript hosts.
 * Define MULIB_IMPL_TERMSCRIPT_TERMINAL and the dependency switches once.
 */
#ifndef MULIB_TERMSCRIPT_TERMINAL_H
#define MULIB_TERMSCRIPT_TERMINAL_H
#include "mulib-termfunc.h"
#include "mulib-termstyle.h"
#include "mulib-termutils.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Accept the full span (1) or decline it (0). An event loop may queue it.
 * The span is borrowed for the call only. Do not reenter the adapter. */
typedef int (*mulib_termscript_write_fn)(void *, const char *, size_t);

typedef struct {
  const mulib_terminfo_t *info;
  mulib_termscript_write_fn write;
  void *context;
  unsigned flags;
  int input_fd;
  mulib_termutils_raw_t raw;
} mulib_termscript_terminal_t;

/* Borrows info and context. input_fd < 0 disables termios. No I/O on init.
 * Restore raw mode before discarding or reinitializing the adapter. */
void mulib_termscript_terminal_init(mulib_termscript_terminal_t *, const mulib_terminfo_t *,
                                    unsigned flags, int input_fd, mulib_termscript_write_fn, void *);
/* Typeout accepts valid UTF-8, permits newline/tab, and replaces all other
 * control scalars. Returns 1/0, with no output on validation failure. */
int mulib_termscript_terminal_text(mulib_termscript_terminal_t *, const char *, size_t);
/* Explicit terminfo by short name; no padding delays or arbitrary sequences. */
int mulib_termscript_terminal_capability(mulib_termscript_terminal_t *, const char *,
                                         size_t, const int32_t params[9]);
int mulib_termscript_terminal_function(mulib_termscript_terminal_t *, mulib_termfunc_op_t,
                                       int32_t, int32_t);
/* Termios calls return MULIB_TERMUTILS_*; restore reports failures for retry. */
int mulib_termscript_terminal_raw(mulib_termscript_terminal_t *);
int mulib_termscript_terminal_restore(mulib_termscript_terminal_t *);
int mulib_termscript_terminal_size(mulib_termscript_terminal_t *, unsigned *, unsigned *);
/* Output callback for scripts running against the repository's VTerm. */
int mulib_termscript_terminal_vterm(void *screen, const char *bytes, size_t length);

#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_TERMSCRIPT_TERMINAL) && !defined(MULIB_TERMSCRIPT_TERMINAL_IMPLEMENTED)
#define MULIB_TERMSCRIPT_TERMINAL_IMPLEMENTED
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

void
mulib_termscript_terminal_init(mulib_termscript_terminal_t *t, const mulib_terminfo_t *info,
                                unsigned flags, int fd, mulib_termscript_write_fn write, void *user)
{
  if (!t) return;
  t->info = info;
  t->write = write;
  t->context = user;
  t->flags = flags;
  t->input_fd = fd;
  t->raw.fd = -1;
  t->raw.saved = NULL;
}

static int
mulib_termscript_terminal_output_(mulib_termscript_terminal_t *t, mulib_strbuf_t *bytes, int ok)
{
  int result = ok && (!bytes->length || t->write(t->context, bytes->data, bytes->length) == 1);
  mulib_strbuf_free(bytes);
  return result;
}

int
mulib_termscript_terminal_text(mulib_termscript_terminal_t *t, const char *text, size_t length)
{
  if (!t || !t->write || length > 1024u * 1024u) return 0;
  mulib_strbuf_t buf = { NULL, 0, 0 };
  int status = mulib_termstyle_write(mulib_term_style_default(), NULL, MULIB_TERMSTYLE_PLAIN,
                                      text, length, &buf);
  return mulib_termscript_terminal_output_(t, &buf, status == MULIB_TERMSTYLE_OK);
}

int
mulib_termscript_terminal_capability(mulib_termscript_terminal_t *t, const char *name,
                                     size_t length, const int32_t params[9])
{
  char cap[32];
  if (!t || !t->write || !t->info || !name || !length || length >= sizeof cap
      || memchr(name, 0, length)) return 0;
  memcpy(cap, name, length);
  cap[length] = 0;
  mulib_strbuf_t buf = { NULL, 0, 0 };
  int ok = mulib_terminfo_emit(t->info, cap, params, &buf);
  return mulib_termscript_terminal_output_(t, &buf, ok);
}

int
mulib_termscript_terminal_function(mulib_termscript_terminal_t *t, mulib_termfunc_op_t op,
                                   int32_t first, int32_t second)
{
  if (!t || !t->write) return 0;
  mulib_strbuf_t buf = { NULL, 0, 0 };
  int status = mulib_termfunc_emit(t->info, t->flags, op, first, second, &buf);
  return mulib_termscript_terminal_output_(t, &buf, status == MULIB_TERMFUNC_OK);
}

int
mulib_termscript_terminal_raw(mulib_termscript_terminal_t *t)
{
  if (!t) return MULIB_TERMUTILS_EINVAL;
  if (t->input_fd < 0) return MULIB_TERMUTILS_ENOTSUP;
  return t->raw.saved ? MULIB_TERMUTILS_OK : mulib_termutils_raw_begin(&t->raw, t->input_fd);
}

int
mulib_termscript_terminal_restore(mulib_termscript_terminal_t *t)
{
  return t ? mulib_termutils_raw_end(&t->raw) : MULIB_TERMUTILS_EINVAL;
}

int
mulib_termscript_terminal_size(mulib_termscript_terminal_t *t, unsigned *columns, unsigned *rows)
{
  if (!t || !columns || !rows) return MULIB_TERMUTILS_EINVAL;
  return t->input_fd < 0 ? MULIB_TERMUTILS_ENOTSUP : mulib_termutils_size(t->input_fd, columns, rows);
}

int
mulib_termscript_terminal_vterm(void *screen, const char *bytes, size_t length)
{
  return mulib_vterm_feed((mulib_vterm_t *)screen, bytes, length);
}

#ifdef __cplusplus
}
#endif
#endif
