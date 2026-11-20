#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define MULIB_IMPL_IMTUI_TERMINAL
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_TERMINFO
#define MULIB_IMPL_TERMKEY
#define MULIB_IMPL_TERMUTILS
#define MULIB_IMPL_UTF8
#include "mulib-imtui-terminal.h"

static void
make_nonblocking (int fd)
{
  int flags = fcntl (fd, F_GETFL);
  assert (flags >= 0 && fcntl (fd, F_SETFL, flags | O_NONBLOCK) == 0);
}

static void
nonblocking_pipe (int fds[2])
{
  assert (pipe (fds) == 0);
  make_nonblocking (fds[0]);
  make_nonblocking (fds[1]);
}

static void
expect_bytes (int fd, const void *bytes, size_t length)
{
  unsigned char actual[128];
  const unsigned char *expected = bytes;
  while (length)
    {
      ssize_t n = read (fd, actual, length < sizeof actual ? length : sizeof actual);
      assert (n > 0 && !memcmp (actual, expected, (size_t)n));
      expected += n;
      length -= (size_t)n;
    }
}

static void
expect_empty (int fd)
{
  char byte;
  assert (read (fd, &byte, 1) < 0 && (errno == EAGAIN || errno == EWOULDBLOCK));
}

static size_t
fill_pipe (int fd, const unsigned char *filler, size_t chunk)
{
  size_t filled = 0;
  ssize_t n;
  while ((n = write (fd, filler, chunk)) > 0)
    {
      assert ((size_t)n == chunk);
      filled += chunk;
    }
  assert (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK));
  while ((n = write (fd, filler, 1)) > 0)
    ++filled;
  assert (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) && filled >= chunk);
  return filled;
}

static void
drain_filler (int fd, const unsigned char *filler, size_t chunk, size_t length)
{
  while (length)
    {
      size_t count = length < chunk ? length : chunk;
      expect_bytes (fd, filler, count);
      length -= count;
    }
}

static void
fragmented_input_and_plain (void)
{
  mulib_imtui_terminal_t s = { 0 };
  mulib_imtui_terminal_options_t opt = MULIB_IMTUI_TERMINAL_OPTIONS_INIT;
  mulib_termkey_event_t event;
  unsigned cols, rows;
  int in[2], out[2], flags;
  char app[] = { 'a', '\0', 'b', 'c' };
  opt.flags |= MULIB_IMTUI_TERMINAL_PLAIN | MULIB_IMTUI_TERMINAL_MOUSE
               | MULIB_IMTUI_TERMINAL_BRACKETED_PASTE;
  opt.fallback_columns = 112;
  opt.fallback_rows = 43;
  opt.output_limit = sizeof app;
  assert (pipe (in) == 0);
  nonblocking_pipe (out);
  assert (mulib_imtui_terminal_attach (&s, in[0], out[1], &opt)
          == MULIB_IMTUI_TERMINAL_EINVAL);
  assert (mulib_imtui_terminal_input_fd (&s) == -1);
  make_nonblocking (in[0]);
  flags = fcntl (in[0], F_GETFL);
  assert (mulib_imtui_terminal_attach (&s, out[1], in[0], &opt)
          == MULIB_IMTUI_TERMINAL_EINVAL);
  assert (mulib_imtui_terminal_attach (&s, in[0], out[1], NULL)
          == MULIB_IMTUI_TERMINAL_EIO); /* Default RAW rejects a pipe. */
  assert (mulib_imtui_terminal_attach (&s, in[0], out[1], &opt) == 0);
  assert (mulib_imtui_terminal_attach (&s, in[0], out[1], &opt)
          == MULIB_IMTUI_TERMINAL_ESTATE);
  assert (mulib_imtui_terminal_input_fd (&s) == in[0]);
  assert (mulib_imtui_terminal_output_fd (&s) == out[1]);
  assert (mulib_imtui_terminal_size (&s, &cols, &rows) == 0 && cols == 112 && rows == 43);
  assert (!mulib_imtui_terminal_output_pending (&s));
  assert (mulib_imtui_terminal_queue (&s, "12345", 5) == MULIB_IMTUI_TERMINAL_TOO_LARGE);
  assert (mulib_imtui_terminal_queue (&s, app, sizeof app) == 0);
  app[0] = 'X'; /* queue owns a copy */
  assert (mulib_imtui_terminal_queue (&s, "x", 1) == MULIB_IMTUI_TERMINAL_AGAIN);
  assert (mulib_imtui_terminal_output_pending (&s));
  assert (mulib_imtui_terminal_flush (&s) == 0);
  assert (!mulib_imtui_terminal_output_pending (&s));
  expect_bytes (out[0], "a\0bc", 4);
  expect_empty (out[0]);

  assert (mulib_imtui_terminal_next (&s, &event) == MULIB_IMTUI_TERMINAL_AGAIN);
  assert (!mulib_imtui_terminal_escape_pending (&s));
  assert (write (in[1], "\033", 1) == 1);
  assert (mulib_imtui_terminal_next (&s, &event) == MULIB_IMTUI_TERMINAL_AGAIN);
  assert (mulib_imtui_terminal_escape_pending (&s));
  assert (write (in[1], "[", 1) == 1);
  assert (mulib_imtui_terminal_next (&s, &event) == MULIB_IMTUI_TERMINAL_AGAIN);
  assert (mulib_imtui_terminal_escape_pending (&s));
  assert (write (in[1], "A", 1) == 1);
  assert (mulib_imtui_terminal_next (&s, &event) == MULIB_IMTUI_TERMINAL_EVENT);
  assert (event.type == MULIB_TERMKEY_KEY && event.key == MULIB_TERMKEY_UP);
  assert (!mulib_imtui_terminal_escape_pending (&s));
  assert (write (in[1], "\033", 1) == 1);
  assert (mulib_imtui_terminal_next (&s, &event) == MULIB_IMTUI_TERMINAL_AGAIN);
  assert (mulib_imtui_terminal_escape_pending (&s));
  assert (write (in[1], "q", 1) == 1);
  assert (mulib_imtui_terminal_flush_input (&s, &event) == MULIB_IMTUI_TERMINAL_EVENT);
  assert (event.type == MULIB_TERMKEY_KEY && event.key == MULIB_TERMKEY_ESCAPE);
  assert (mulib_imtui_terminal_flush_input (&s, &event) == MULIB_IMTUI_TERMINAL_AGAIN);
  assert (mulib_imtui_terminal_next (&s, &event) == MULIB_IMTUI_TERMINAL_EVENT);
  assert (event.type == MULIB_TERMKEY_KEY && event.codepoint == 'q' && !event.modifiers);

  assert (write (in[1], "\342\202", 2) == 2);
  assert (mulib_imtui_terminal_next (&s, &event) == MULIB_IMTUI_TERMINAL_AGAIN);
  assert (!mulib_imtui_terminal_escape_pending (&s));
  assert (write (in[1], "\254", 1) == 1);
  assert (mulib_imtui_terminal_next (&s, &event) == MULIB_IMTUI_TERMINAL_EVENT);
  assert (event.key == MULIB_TERMKEY_CHAR && event.codepoint == 0x20ac);
  assert (write (in[1], "\033[200~hi\033[20", 12) == 12);
  assert (mulib_imtui_terminal_next (&s, &event) == MULIB_IMTUI_TERMINAL_EVENT);
  assert (event.type == MULIB_TERMKEY_PASTE_BEGIN);
  assert (mulib_imtui_terminal_next (&s, &event) == MULIB_IMTUI_TERMINAL_EVENT);
  assert (event.type == MULIB_TERMKEY_PASTE_TEXT && event.length == 2
          && !memcmp (event.raw, "hi", 2));
  assert (mulib_imtui_terminal_next (&s, &event) == MULIB_IMTUI_TERMINAL_AGAIN);
  assert (!mulib_imtui_terminal_escape_pending (&s));
  assert (write (in[1], "1~\033[<0;3;4M", 11) == 11);
  assert (mulib_imtui_terminal_next (&s, &event) == MULIB_IMTUI_TERMINAL_EVENT);
  assert (event.type == MULIB_TERMKEY_PASTE_END);
  assert (mulib_imtui_terminal_next (&s, &event) == MULIB_IMTUI_TERMINAL_EVENT);
  assert (event.type == MULIB_TERMKEY_MOUSE && event.mouse_action == MULIB_TERMKEY_PRESS
          && event.mouse_button == 1 && event.x == 2 && event.y == 3);

  /* A full input chunk must not stall even when it fills termkey's buffer. */
  {
    char batch[MULIB_TERMKEY_CAPACITY];
    size_t i;
    memset (batch, 'x', sizeof batch);
    assert (write (in[1], batch, sizeof batch) == (ssize_t)sizeof batch);
    for (i = 0; i < sizeof batch; ++i)
      {
        assert (mulib_imtui_terminal_next (&s, &event) == MULIB_IMTUI_TERMINAL_EVENT);
        assert (event.key == MULIB_TERMKEY_CHAR && event.codepoint == 'x');
      }
  }
  assert (write (in[1], "\033", 1) == 1 && close (in[1]) == 0);
  assert (mulib_imtui_terminal_next (&s, &event) == MULIB_IMTUI_TERMINAL_EVENT);
  assert (event.key == MULIB_TERMKEY_ESCAPE);
  assert (mulib_imtui_terminal_next (&s, &event) == MULIB_IMTUI_TERMINAL_EOF);
  assert (mulib_imtui_terminal_next (&s, &event) == MULIB_IMTUI_TERMINAL_EOF);
  assert (mulib_imtui_terminal_close (&s) == 0 && mulib_imtui_terminal_close (&s) == 0);
  assert (mulib_imtui_terminal_input_fd (&s) == -1 && fcntl (in[0], F_GETFL) == flags);
  expect_empty (out[0]);
  assert (close (in[0]) == 0 && close (out[0]) == 0 && close (out[1]) == 0);
}

static void
modes_and_abort (void)
{
  static const char enter[] = "\033[?1049h\033[?25l\033[?1006h\033[?1002h\033[?2004h";
  static const char leave[] = "\033[?2004l\033[?1002l\033[?1006l\033[?25h\033[?1049l";
  mulib_imtui_terminal_t s = { 0 };
  mulib_imtui_terminal_options_t opt = {
    MULIB_IMTUI_TERMINAL_ALT_SCREEN | MULIB_IMTUI_TERMINAL_HIDE_CURSOR
        | MULIB_IMTUI_TERMINAL_MOUSE | MULIB_IMTUI_TERMINAL_BRACKETED_PASTE,
    0, 0, 1, NULL
  };
  unsigned cols = 7, rows = 9;
  int in[2], out[2];
  nonblocking_pipe (in);
  nonblocking_pipe (out);
  assert (mulib_imtui_terminal_attach (&s, in[0], out[1], &opt) == 0);
  assert (mulib_imtui_terminal_size (&s, &cols, &rows) == MULIB_IMTUI_TERMINAL_EIO
          && cols == 7 && rows == 9);
  expect_empty (out[0]); /* Opening is not a write. */
  assert (mulib_imtui_terminal_queue (&s, "F", 1) == 0);
  assert (mulib_imtui_terminal_close (&s) == 0);
  expect_bytes (out[0], enter, sizeof enter - 1);
  expect_bytes (out[0], "F", 1);
  expect_bytes (out[0], leave, sizeof leave - 1);
  expect_empty (out[0]);

  assert (mulib_imtui_terminal_attach (&s, in[0], out[1], &opt) == 0);
  assert (mulib_imtui_terminal_abort (&s) == 0);
  expect_empty (out[0]);
  assert (mulib_imtui_terminal_attach (&s, in[0], out[1], &opt) == 0);
  assert (mulib_imtui_terminal_flush (&s) == 0);
  expect_bytes (out[0], enter, sizeof enter - 1);
  assert (mulib_imtui_terminal_queue (&s, "F", 1) == 0);
  assert (mulib_imtui_terminal_abort (&s) == 0 && mulib_imtui_terminal_abort (&s) == 0);
  expect_bytes (out[0], leave, sizeof leave - 1);
  expect_empty (out[0]);
  assert (close (in[0]) == 0 && close (in[1]) == 0);
  assert (close (out[0]) == 0 && close (out[1]) == 0);
}

static void
store16 (unsigned char *p, unsigned n)
{
  p[0] = (unsigned char)n;
  p[1] = (unsigned char)(n >> 8);
}

static void
terminfo_modes (void)
{
  /* Compiled little-endian core record: only the alternate-screen pair and
   * civis exist (no cnorm). The enter capability includes a padding delay. */
  static const char strings[] = "in$<5>\0out\0hide";
  unsigned char record[14 + 41 * 2 + sizeof strings] = { 0 };
  mulib_terminfo_t info;
  mulib_imtui_terminal_t s = { 0 };
  mulib_imtui_terminal_options_t opt = {
    MULIB_IMTUI_TERMINAL_ALT_SCREEN | MULIB_IMTUI_TERMINAL_HIDE_CURSOR,
    0, 0, 0, &info
  };
  int in[2], out[2];
  store16 (record, 0x011a);
  store16 (record + 2, 2);
  store16 (record + 8, 41);
  store16 (record + 10, sizeof strings);
  record[12] = 't';
  memset (record + 14, 0xff, 41 * 2);
  store16 (record + 14 + 2 * 13, 11); /* civis */
  store16 (record + 14 + 2 * 28, 0);  /* smcup */
  store16 (record + 14 + 2 * 40, 7);  /* rmcup */
  memcpy (record + 14 + 41 * 2, strings, sizeof strings);
  mulib_terminfo_init (&info);
  nonblocking_pipe (in);
  nonblocking_pipe (out);
  assert (mulib_terminfo_load (&info, record, sizeof record));
  assert (mulib_imtui_terminal_attach (&s, in[0], out[1], &opt) == 0);
  mulib_terminfo_free (&info); /* Session owns expanded bytes, not this record. */
  assert (mulib_imtui_terminal_close (&s) == 0);
  expect_bytes (out[0], "inout", 5);
  expect_empty (out[0]);
  record[14 + 41 * 2] = '%'; /* Unsupported %n in smcup. */
  assert (mulib_terminfo_load (&info, record, sizeof record));
  assert (mulib_imtui_terminal_attach (&s, in[0], out[1], &opt) == MULIB_IMTUI_TERMINAL_ECAP);
  assert (mulib_imtui_terminal_output_fd (&s) == -1);
  expect_empty (out[0]);
  mulib_terminfo_free (&info);
  assert (close (in[0]) == 0 && close (in[1]) == 0);
  assert (close (out[0]) == 0 && close (out[1]) == 0);
}

static void
short_writes_and_resumable_close (void)
{
  static const char tail[] = "!\033[?2004l";
  mulib_imtui_terminal_t s = { 0 };
  mulib_imtui_terminal_options_t opt = {
    MULIB_IMTUI_TERMINAL_BRACKETED_PASTE, 0, 0, 0, NULL
  };
  int in[2], out[2], rc;
  long pipe_buf;
  size_t chunk, frame_length, filled, pos = 0, total, i;
  unsigned char *filler, *frame;
  nonblocking_pipe (in);
  nonblocking_pipe (out);
  pipe_buf = fpathconf (out[1], _PC_PIPE_BUF);
  assert (pipe_buf > 0 && (unsigned long)pipe_buf <= 1048576ul);
  chunk = (size_t)pipe_buf;
  frame_length = chunk * 2;
  total = frame_length + sizeof tail - 1;
  opt.output_limit = frame_length;
  filler = malloc (chunk);
  frame = malloc (total);
  assert (filler && frame);
  memset (filler, 'f', chunk);
  for (i = 0; i < frame_length; ++i)
    frame[i] = (unsigned char)(i % 239u);
  memcpy (frame + frame_length, tail, sizeof tail - 1);
  assert (mulib_imtui_terminal_attach (&s, in[0], out[1], &opt) == 0);
  assert (mulib_imtui_terminal_flush (&s) == 0);
  expect_bytes (out[0], "\033[?2004h", 8);
  filled = fill_pipe (out[1], filler, chunk);
  expect_bytes (out[0], filler, chunk);
  assert (mulib_imtui_terminal_queue (&s, frame, frame_length) == 0);
  assert (mulib_imtui_terminal_flush (&s) == MULIB_IMTUI_TERMINAL_AGAIN);
  /* The short write frees app queue budget, and appending compacts the suffix. */
  assert (mulib_imtui_terminal_queue (&s, "!", 1) == 0);
  drain_filler (out[0], filler, chunk, filled - chunk);
  do
    {
      unsigned char actual[256];
      size_t start = pos;
      ssize_t n;
      while ((n = read (out[0], actual, sizeof actual)) > 0)
        {
          assert ((size_t)n <= total - pos && !memcmp (actual, frame + pos, (size_t)n));
          pos += (size_t)n;
        }
      assert (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK) && pos > start);
      rc = mulib_imtui_terminal_close (&s);
    }
  while (rc == MULIB_IMTUI_TERMINAL_AGAIN);
  assert (rc == 0);
  expect_bytes (out[0], frame + pos, total - pos);
  expect_empty (out[0]);

  /* Force cleanup itself to block, independently of the host pipe capacity. */
  assert (mulib_imtui_terminal_attach (&s, in[0], out[1], &opt) == 0);
  assert (mulib_imtui_terminal_flush (&s) == 0);
  expect_bytes (out[0], "\033[?2004h", 8);
  filled = fill_pipe (out[1], filler, chunk);
  assert (mulib_imtui_terminal_close (&s) == MULIB_IMTUI_TERMINAL_AGAIN);
  assert (mulib_imtui_terminal_output_pending (&s));
  assert (mulib_imtui_terminal_output_fd (&s) == out[1]);
  assert (mulib_imtui_terminal_flush (&s) == MULIB_IMTUI_TERMINAL_ESTATE);
  assert (mulib_imtui_terminal_queue (&s, "no", 2) == MULIB_IMTUI_TERMINAL_ESTATE);
  drain_filler (out[0], filler, chunk, filled);
  assert (mulib_imtui_terminal_close (&s) == 0);
  expect_bytes (out[0], "\033[?2004l", 8);
  expect_empty (out[0]);
  free (frame);
  free (filler);
  assert (close (in[0]) == 0 && close (in[1]) == 0);
  assert (close (out[0]) == 0 && close (out[1]) == 0);
}

#if defined(_XOPEN_UNIX) && _XOPEN_UNIX > 0 && defined(TIOCSWINSZ)
#define TEST_IMTUI_TERMINAL_PTY
static void
expect_restored (int fd, const struct termios *before)
{
  struct termios after;
  assert (tcgetattr (fd, &after) == 0);
  assert (after.c_iflag == before->c_iflag && after.c_oflag == before->c_oflag
          && after.c_cflag == before->c_cflag && after.c_lflag == before->c_lflag);
  assert (!memcmp (after.c_cc, before->c_cc, sizeof after.c_cc));
}

static void
owned_pty (void)
{
  mulib_imtui_terminal_t s = { 0 };
  mulib_imtui_terminal_options_t opt = MULIB_IMTUI_TERMINAL_OPTIONS_INIT;
  struct termios before, during;
  struct winsize size = { 37, 91, 0, 0 };
  mulib_termkey_event_t event;
  unsigned cols, rows;
  int master = posix_openpt (O_RDWR | O_NOCTTY), slave, owned;
  const char *path;
  struct pollfd ready;
  if (master < 0)
    return; /* /dev/ptmx may be unavailable in restricted test environments. */
  make_nonblocking (master);
  assert (grantpt (master) == 0 && unlockpt (master) == 0);
  path = ptsname (master);
  assert (path);
  slave = open (path, O_RDWR | O_NOCTTY);
  assert (slave >= 0 && tcgetattr (slave, &before) == 0);
  assert (ioctl (slave, TIOCSWINSZ, &size) == 0);
  assert (mulib_imtui_terminal_open (&s, "/dev/null", &opt) == MULIB_IMTUI_TERMINAL_EIO);
  opt.flags = MULIB_IMTUI_TERMINAL_RAW;
  assert (mulib_imtui_terminal_open (&s, path, &opt) == 0);
  owned = mulib_imtui_terminal_input_fd (&s);
  assert (owned >= 0 && owned == mulib_imtui_terminal_output_fd (&s));
  assert (fcntl (owned, F_GETFL) & O_NONBLOCK);
  assert (fcntl (owned, F_GETFD) & FD_CLOEXEC);
  assert (tcgetattr (slave, &during) == 0);
  assert (!(during.c_lflag & (ECHO | ICANON | ISIG | IEXTEN))
          && !(during.c_oflag & OPOST) && !(during.c_iflag & (ICRNL | IXON)));
  assert (during.c_cc[VMIN] == 1 && during.c_cc[VTIME] == 0);
  assert (mulib_imtui_terminal_size (&s, &cols, &rows) == 0 && cols == 91 && rows == 37);
  size.ws_col = 103;
  size.ws_row = 49;
  assert (ioctl (slave, TIOCSWINSZ, &size) == 0);
  assert (mulib_imtui_terminal_size (&s, &cols, &rows) == 0 && cols == 103 && rows == 49);
  assert (mulib_imtui_terminal_next (&s, &event) == MULIB_IMTUI_TERMINAL_AGAIN);
  assert (write (master, "\003", 1) == 1);
  ready = (struct pollfd){ owned, POLLIN, 0 };
  assert (poll (&ready, 1, 1000) == 1 && (ready.revents & POLLIN));
  assert (mulib_imtui_terminal_next (&s, &event) == MULIB_IMTUI_TERMINAL_EVENT);
  assert (event.key == MULIB_TERMKEY_CHAR && event.codepoint == 'c'
          && event.modifiers == MULIB_TERMKEY_CTRL);
  assert (mulib_imtui_terminal_close (&s) == 0);
  assert (fcntl (owned, F_GETFD) < 0 && errno == EBADF);
  expect_restored (slave, &before);
  assert (mulib_imtui_terminal_open (&s, path, &opt) == 0);
  assert (mulib_imtui_terminal_abort (&s) == 0);
  expect_restored (slave, &before);

  {
    int out[2];
    unsigned char filler[512] = { 0 };
    nonblocking_pipe (out);
    make_nonblocking (slave);
    opt.flags |= MULIB_IMTUI_TERMINAL_BRACKETED_PASTE;
    assert (mulib_imtui_terminal_attach (&s, slave, out[1], &opt) == 0);
    assert (mulib_imtui_terminal_size (&s, &cols, &rows) == 0
            && cols == 103 && rows == 49); /* Falls back to input tty. */
    assert (mulib_imtui_terminal_flush (&s) == 0);
    expect_bytes (out[0], "\033[?2004h", 8);
    (void)fill_pipe (out[1], filler, sizeof filler);
    assert (mulib_imtui_terminal_close (&s) == MULIB_IMTUI_TERMINAL_AGAIN);
    assert (tcgetattr (slave, &during) == 0 && !(during.c_lflag & ICANON));
    assert (mulib_imtui_terminal_abort (&s) == MULIB_IMTUI_TERMINAL_EIO);
    assert (errno == EAGAIN || errno == EWOULDBLOCK);
    expect_restored (slave, &before); /* Failed escape restoration is separate. */
    assert (mulib_imtui_terminal_input_fd (&s) == -1);
    assert (close (out[0]) == 0 && close (out[1]) == 0);
  }
  assert (close (slave) == 0 && close (master) == 0);
}
#endif

int
main (void)
{
  fragmented_input_and_plain ();
  modes_and_abort ();
  terminfo_modes ();
  short_writes_and_resumable_close ();
#ifdef TEST_IMTUI_TERMINAL_PTY
  owned_pty ();
#endif
  return 0;
}
