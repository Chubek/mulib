/* Standalone POSIX ImTUI application. Q, Escape, or the Quit button exits. */
#define _POSIX_C_SOURCE 200809L
#define MULIB_IMPL_GRID
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_VTERM
#define MULIB_IMPL_TERMKEY
#define MULIB_IMPL_TERMUTILS
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_TERMINFO
#define MULIB_IMPL_XML
#define MULIB_IMPL_CSS
#define MULIB_IMPL_TERMSTYLE
#define MULIB_IMPL_IMTUI_CORE
#define MULIB_IMPL_IMTUI_DRAW
#define MULIB_IMPL_IMTUI_RENDER
#define MULIB_IMPL_IMTUI_TERMINAL
#define MULIB_IMPL_IMTUI_WIDGETS
#include "mulib-imtui-render.h"
#include "mulib-imtui-terminal.h"
#include "mulib-imtui-widgets.h"
#include <stdlib.h>

#if defined(__unix__) || defined(__APPLE__)
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>

static volatile sig_atomic_t stop;

static void
stop_signal(int signum)
{
  (void)signum;
  stop = 1;
}

static int
wait_io(const mulib_imtui_terminal_t *term, short events, int timeout)
{
  int fd = events == POLLOUT ? mulib_imtui_terminal_output_fd(term)
    : mulib_imtui_terminal_input_fd(term);
  struct pollfd pfd = { fd, events, 0 };
  int status;
  do status = poll(&pfd, 1, timeout);
  while (status < 0 && errno == EINTR && !stop);
  return status;
}

static int
drain(mulib_imtui_terminal_t *term)
{
  int status;
  while (!stop) {
    status = mulib_imtui_terminal_flush(term);
    if (status != MULIB_IMTUI_TERMINAL_AGAIN) return status == MULIB_IMTUI_TERMINAL_OK;
    if (wait_io(term, POLLOUT, 100) < 0) return 0;
  }
  return 1;
}

static int
frame(mulib_imtui_t *ui, mulib_imtui_terminal_t *term, mulib_imtui_renderer_t *r,
      const mulib_terminfo_t *info, const mulib_termkey_event_t *input, size_t n, int *setting)
{
  unsigned w, h;
  int resized = 0;
  if (mulib_imtui_terminal_size(term, &w, &h) != MULIB_IMTUI_TERMINAL_OK || !w || !h)
    return 0;
  if (w > 160) w = 160;
  if (h > 60) h = 60;
  mulib_imtui_rect_t old = mulib_imtui_bounds(ui);
  if ((int)w != old.w || (int)h != old.h) {
    if (!mulib_imtui_resize(ui, (int)w, (int)h)) return 0;
    mulib_imtui_renderer_invalidate(r);
    resized = 1;
  }
  if (!mulib_imtui_begin(ui, input, n)) return 0;
  mulib_term_style_t title = mulib_term_style_default();
  title.attributes = MULIB_TERM_BOLD;
  (void)mulib_imtui_text(ui, 1, 0, "ImTUI / settings", 16, title);
  (void)mulib_imtui_checkbox(ui, mulib_imtui_id(ui, "updates", 7),
    (mulib_imtui_rect_t){ 1, 2, 24, 1 }, "Show updates", 12, setting, 1);
  if (mulib_imtui_button(ui, mulib_imtui_id(ui, "quit", 4),
                         (mulib_imtui_rect_t){ 1, 4, 12, 1 }, "Quit", 4, 1))
    stop = 1;
  (void)mulib_imtui_text(ui, 1, 6, "Tab / Space / Q", 15, mulib_term_style_default());
  mulib_imtui_end(ui);
  mulib_strbuf_t bytes = { NULL, 0, 0 };
  int ok = (!resized || mulib_terminfo_emit(info, "clear", NULL, &bytes))
    && mulib_imtui_render(r, ui, info, 0, &bytes)
    && mulib_imtui_terminal_queue(term, bytes.data, bytes.length) == MULIB_IMTUI_TERMINAL_OK
    && drain(term);
  mulib_strbuf_free(&bytes);
  return ok;
}

int
main(void)
{
  struct sigaction sa = { 0 };
  sa.sa_handler = stop_signal;
  if (sigemptyset(&sa.sa_mask) || sigaction(SIGINT, &sa, NULL)
      || sigaction(SIGTERM, &sa, NULL) || sigaction(SIGHUP, &sa, NULL))
    return EXIT_FAILURE;
  mulib_imtui_terminal_options_t opts = MULIB_IMTUI_TERMINAL_OPTIONS_INIT;
  opts.flags |= MULIB_IMTUI_TERMINAL_MOUSE;
  opts.output_limit = 1024 * 1024;
  mulib_terminfo_t info;
  mulib_terminfo_init(&info);
  mulib_terminfo_ansi(&info);
  opts.terminfo = &info;
  mulib_imtui_terminal_t term = { 0 };
  if (mulib_imtui_terminal_open(&term, NULL, &opts) != MULIB_IMTUI_TERMINAL_OK) {
    fputs("ImTUI demo needs a foreground POSIX tty.\n", stderr);
    return EXIT_FAILURE;
  }
  mulib_imtui_t ui;
  mulib_imtui_renderer_t renderer;
  mulib_imtui_renderer_init(&renderer);
  mulib_termkey_event_t events[64];
  size_t n = 0;
  int setting = 1, ok = mulib_imtui_init(&ui, 1, 1);
  while (ok && !stop) {
    ok = frame(&ui, &term, &renderer, &info, events, n, &setting);
    if (!ok || stop) break;
    n = 0;
    while (n < sizeof events / sizeof *events) {
      mulib_termkey_event_t *event = events + n;
      int status = mulib_imtui_terminal_next(&term, event);
      if (status == MULIB_IMTUI_TERMINAL_AGAIN && !n) {
        int escape = mulib_imtui_terminal_escape_pending(&term);
        int ready = wait_io(&term, POLLIN, escape ? 35 : 100);
        if (stop) break;
        if (ready < 0) { ok = 0; break; }
        if (ready > 0) continue;
        if (escape) status = mulib_imtui_terminal_flush_input(&term, event);
      }
      if (status < 0) { ok = 0; break; }
      if (status == MULIB_IMTUI_TERMINAL_EOF) { stop = 1; break; }
      if (status != MULIB_IMTUI_TERMINAL_EVENT) break;
      if (event->type == MULIB_TERMKEY_KEY && (event->key == MULIB_TERMKEY_ESCAPE
          || (event->key == MULIB_TERMKEY_CHAR && (event->codepoint == 'q'
          || (event->codepoint == 'c' && (event->modifiers & MULIB_TERMKEY_CTRL)))))) {
        stop = 1;
        break;
      }
      ++n;
    }
  }
  int status = mulib_imtui_terminal_close(&term);
  if (status == MULIB_IMTUI_TERMINAL_AGAIN) {
    if (wait_io(&term, POLLOUT, 100) > 0) status = mulib_imtui_terminal_close(&term);
    if (status == MULIB_IMTUI_TERMINAL_AGAIN) status = mulib_imtui_terminal_abort(&term);
  }
  mulib_imtui_renderer_free(&renderer);
  mulib_imtui_free(&ui);
  mulib_terminfo_free(&info);
  return ok && status == MULIB_IMTUI_TERMINAL_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
#else
int main(void) { return EXIT_FAILURE; }
#endif
