#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include "../stdlib/tty/runtime.h"
#include "mulib-termkey.h"
#include "mulib-vterm.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#endif

static void
test_decoder(void)
{
  int64_t handle = on1x_tty_decoder_open();
  const char *raw;
  char too_long[MULIB_TERMKEY_CAPACITY + 2u];
  char too_much_hex[MULIB_TERMKEY_CAPACITY * 2u + 2u];
  assert(handle > 0 && on1x_tty_status() == ON1X_TTY_OK);
  assert(on1x_tty_decoder_feed_hex(handle, "1b5b313b3541"));
  assert(on1x_tty_decoder_next(handle, false));
  assert(on1x_tty_decoder_event_value(handle, ON1X_TTY_EVENT_TYPE)
         == MULIB_TERMKEY_KEY);
  assert(on1x_tty_decoder_event_value(handle, ON1X_TTY_EVENT_KEY)
         == MULIB_TERMKEY_UP);
  assert(on1x_tty_decoder_event_value(handle, ON1X_TTY_EVENT_MODIFIERS)
         == MULIB_TERMKEY_CTRL);
  raw = on1x_tty_decoder_event_raw_hex(handle);
  assert(!strcmp(raw, "1b5b313b3541"));
  assert(!on1x_tty_decoder_next(handle, false));
  assert(on1x_tty_status() == ON1X_TTY_AGAIN);
  assert(!strcmp(on1x_tty_decoder_event_raw_hex(handle), ""));
  assert(on1x_tty_status() == ON1X_TTY_INVALID);

  assert(on1x_tty_decoder_feed_hex(handle, "1b5b3c303b333b344d"));
  assert(on1x_tty_decoder_next(handle, false));
  assert(on1x_tty_decoder_event_value(handle, ON1X_TTY_EVENT_TYPE)
         == MULIB_TERMKEY_MOUSE);
  assert(on1x_tty_decoder_event_value(handle, ON1X_TTY_EVENT_MOUSE_BUTTON)
         == 1);
  assert(on1x_tty_decoder_event_value(handle, ON1X_TTY_EVENT_MOUSE_ACTION)
         == MULIB_TERMKEY_PRESS);
  assert(on1x_tty_decoder_event_value(handle, ON1X_TTY_EVENT_X) == 2);
  assert(on1x_tty_decoder_event_value(handle, ON1X_TTY_EVENT_Y) == 3);
  assert(on1x_tty_decoder_feed_hex(handle, "ff"));
  assert(on1x_tty_decoder_next(handle, false));
  assert(on1x_tty_decoder_event_value(handle, ON1X_TTY_EVENT_KEY)
         == MULIB_TERMKEY_CHAR);
  assert(on1x_tty_decoder_event_value(handle, ON1X_TTY_EVENT_CODEPOINT)
         == 0xfffd);

  assert(on1x_tty_decoder_feed_hex(
      handle, "1b5b3230307e00ff1b5b3230317e"));
  assert(on1x_tty_decoder_next(handle, false));
  assert(on1x_tty_decoder_event_value(handle, ON1X_TTY_EVENT_TYPE)
         == MULIB_TERMKEY_PASTE_BEGIN);
  assert(on1x_tty_decoder_next(handle, false));
  assert(on1x_tty_decoder_event_value(handle, ON1X_TTY_EVENT_TYPE)
         == MULIB_TERMKEY_PASTE_TEXT);
  assert(!strcmp(on1x_tty_decoder_event_raw_hex(handle), "00ff"));
  assert(on1x_tty_decoder_next(handle, false));
  assert(on1x_tty_decoder_event_value(handle, ON1X_TTY_EVENT_TYPE)
         == MULIB_TERMKEY_PASTE_END);

  assert(!on1x_tty_decoder_feed_hex(handle, "0"));
  assert(on1x_tty_status() == ON1X_TTY_INVALID);
  memset(too_much_hex, '0', sizeof(too_much_hex));
  too_much_hex[sizeof(too_much_hex) - 1u] = '\0';
  assert(!on1x_tty_decoder_feed_hex(handle, too_much_hex));
  assert(on1x_tty_status() == ON1X_TTY_LIMIT);
  memset(too_long, 'x', sizeof(too_long));
  too_long[sizeof(too_long) - 1u] = '\0';
  assert(!on1x_tty_decoder_feed(handle, too_long));
  assert(on1x_tty_status() == ON1X_TTY_LIMIT);
  assert(on1x_tty_decoder_reset(handle));
  assert(on1x_tty_decoder_feed(handle, "\033"));
  assert(!on1x_tty_decoder_next(handle, false));
  assert(on1x_tty_status() == ON1X_TTY_AGAIN);
  assert(on1x_tty_decoder_next(handle, true));
  assert(on1x_tty_decoder_event_value(handle, ON1X_TTY_EVENT_KEY)
         == MULIB_TERMKEY_ESCAPE);
  assert(on1x_tty_decoder_close(handle));
  assert(!on1x_tty_decoder_close(handle));
  assert(on1x_tty_status() == ON1X_TTY_INVALID);
  assert(!strcmp(raw, "1b5b313b3541")); /* Snapshots outlive handle reuse. */
}

static void
test_decoder_limit(void)
{
  enum { decoder_slots = 32 };
  int64_t handles[decoder_slots + 1u];
  size_t i;
  for (i = 0; i < decoder_slots; ++i)
    {
      handles[i] = on1x_tty_decoder_open();
      assert(handles[i] > 0);
    }
  handles[decoder_slots] = on1x_tty_decoder_open();
  assert(handles[decoder_slots] == -1);
  assert(on1x_tty_status() == ON1X_TTY_LIMIT);
  for (i = 0; i < decoder_slots; ++i)
    assert(on1x_tty_decoder_close(handles[i]));
  handles[0] = on1x_tty_decoder_open();
  assert(handles[0] > 0);
  assert(on1x_tty_decoder_close(handles[0]));
}

static void
test_vterm(void)
{
  int64_t handle = on1x_tty_vterm_open(5, 2);
  int64_t cursor;
  assert(handle > 0 && on1x_tty_status() == ON1X_TTY_OK);
  assert(on1x_tty_vterm_feed_hex(handle, "1b5b33316d58"));
  assert(on1x_tty_vterm_cell(handle, 0, 0, ON1X_TTY_CELL_CODEPOINT) == 'X');
  assert(on1x_tty_vterm_cell(handle, 0, 0, ON1X_TTY_CELL_FOREGROUND) == 1);
  assert(on1x_tty_vterm_cell(handle, 0, 0, ON1X_TTY_CELL_BACKGROUND)
         == UINT32_MAX);
  assert(on1x_tty_vterm_value(handle, ON1X_TTY_VTERM_WIDTH) == 5);
  assert(on1x_tty_vterm_value(handle, ON1X_TTY_VTERM_HEIGHT) == 2);
  cursor = on1x_tty_vterm_value(handle, ON1X_TTY_VTERM_CURSOR_X);
  assert(!on1x_tty_vterm_feed_hex(handle, "xyz"));
  assert(on1x_tty_status() == ON1X_TTY_INVALID);
  assert(on1x_tty_vterm_value(handle, ON1X_TTY_VTERM_CURSOR_X) == cursor);
  assert(on1x_tty_vterm_feed_hex(handle, "e2"));
  assert(on1x_tty_vterm_finish(handle));
  assert(on1x_tty_vterm_value(handle, ON1X_TTY_VTERM_INVALID_INPUT) == 1);
  assert(on1x_tty_vterm_resize(handle, 6, 3));
  assert(on1x_tty_vterm_value(handle, ON1X_TTY_VTERM_WIDTH) == 6);
  assert(!on1x_tty_vterm_resize(handle, 1048576, 2));
  assert(on1x_tty_status() == ON1X_TTY_INVALID);
  assert(on1x_tty_vterm_reset(handle));
  assert(on1x_tty_vterm_cell(handle, 0, 0, ON1X_TTY_CELL_CODEPOINT) == ' ');
  assert(on1x_tty_vterm_cell(handle, 99, 0, ON1X_TTY_CELL_CODEPOINT) == -1);
  assert(on1x_tty_status() == ON1X_TTY_INVALID);
  assert(on1x_tty_vterm_close(handle));
  assert(on1x_tty_vterm_value(handle, ON1X_TTY_VTERM_WIDTH) == -1);
  assert(on1x_tty_status() == ON1X_TTY_INVALID);
  assert(on1x_tty_vterm_open(0, 1) == -1);
  assert(on1x_tty_status() == ON1X_TTY_INVALID);
}

#if defined(__unix__) || defined(__APPLE__)
static int
open_terminal(int *master)
{
  int slave;
  *master = posix_openpt(O_RDWR | O_NOCTTY);
  assert(*master >= 0);
  assert(grantpt(*master) == 0);
  assert(unlockpt(*master) == 0);
  slave = open(ptsname(*master), O_RDWR | O_NOCTTY);
  assert(slave >= 0);
  return slave;
}

static void
assert_restored(const struct termios *before, const struct termios *after)
{
  assert(before->c_iflag == after->c_iflag);
  assert(before->c_oflag == after->c_oflag);
  assert(before->c_cflag == after->c_cflag);
  assert(before->c_lflag == after->c_lflag);
  assert(before->c_cc[VMIN] == after->c_cc[VMIN]);
  assert(before->c_cc[VTIME] == after->c_cc[VTIME]);
}

static void
store16(unsigned char *bytes, unsigned value)
{
  bytes[0] = (unsigned char)value;
  bytes[1] = (unsigned char)(value >> 8);
}

static void
test_compiled_terminfo(int slave)
{
  static const struct
  {
    unsigned index;
    const char *value;
  } strings[] = {
    { 27, "\033[1m" },
    { 39, "\033[0m" },
    { 297, "\033[39;49m" },
    { 359, "\033[38;5;%p1%dm" },
    { 360, "\033[48;5;%p1%dm" }
  };
  static const char names[] = "on1x-test|test";
  enum { number_count = 14, string_count = 361, table_capacity = 128 };
  unsigned char record[12u + sizeof(names) + 1u + number_count * 2u
                       + string_count * 2u + table_capacity];
  size_t offset = 12u, number_offset, string_offset, table_offset, table_size = 0;
  char root[] = "on1x-tty-info-XXXXXX";
  char directory[128], path[160];
  FILE *file;
  int count;
  int64_t caps;
  const char *rendered;
  size_t i;

  memset(record, 0, sizeof(record));
  store16(record, 0x011a);
  store16(record + 2, (unsigned)sizeof(names));
  store16(record + 4, 0);
  store16(record + 6, number_count);
  store16(record + 8, string_count);
  memcpy(record + offset, names, sizeof(names));
  offset += sizeof(names);
  if (offset & 1u)
    ++offset;
  number_offset = offset;
  memset(record + number_offset, 0xff, number_count * 2u);
  store16(record + number_offset + 13u * 2u, 256);
  offset += number_count * 2u;
  string_offset = offset;
  memset(record + string_offset, 0xff, string_count * 2u);
  offset += string_count * 2u;
  table_offset = offset;
  for (i = 0; i < sizeof(strings) / sizeof(strings[0]); ++i)
    {
      size_t length = strlen(strings[i].value) + 1u;
      assert(length <= table_capacity - table_size);
      store16(record + string_offset + strings[i].index * 2u,
              (unsigned)table_size);
      memcpy(record + table_offset + table_size, strings[i].value, length);
      table_size += length;
    }
  store16(record + 10, (unsigned)table_size);

  assert(mkdtemp(root));
  count = snprintf(directory, sizeof(directory), "%s/o", root);
  assert(count > 0 && (size_t)count < sizeof(directory));
  assert(mkdir(directory, 0700) == 0);
  count = snprintf(path, sizeof(path), "%s/on1x-test", directory);
  assert(count > 0 && (size_t)count < sizeof(path));
  file = fopen(path, "wb");
  assert(file);
  assert(fwrite(record, 1, table_offset + table_size, file)
         == table_offset + table_size);
  assert(fclose(file) == 0);

  assert(setenv("TERMINFO", root, 1) == 0);
  assert(setenv("TERM", "on1x-test", 1) == 0);
  assert(setenv("COLORTERM", "truecolor", 1) == 0);
  assert(unsetenv("NO_COLOR") == 0);
  caps = on1x_tty_capabilities(slave);
  assert(on1x_tty_status() == ON1X_TTY_OK);
  assert((caps & (ON1X_TTY_CAP_TTY | ON1X_TTY_CAP_ESCAPES
                  | ON1X_TTY_CAP_COLOR | ON1X_TTY_CAP_TERMINFO))
         == (ON1X_TTY_CAP_TTY | ON1X_TTY_CAP_ESCAPES | ON1X_TTY_CAP_COLOR
             | ON1X_TTY_CAP_TERMINFO));
  assert(!(caps & (ON1X_TTY_CAP_TRUECOLOR | ON1X_TTY_CAP_ANSI_FALLBACK)));
  rendered = on1x_tty_render(slave, "loaded", MULIB_TERM_BOLD, 2, -1, true);
  assert(on1x_tty_status() == ON1X_TTY_OK);
  assert(strstr(rendered, "\033[1m") && strstr(rendered, "\033[38;5;2m"));
  assert(!strcmp(on1x_tty_control(
                     slave, ON1X_TTY_CONTROL_PASTE_ENTER, 0, 0),
                 ""));
  assert(on1x_tty_status() == ON1X_TTY_UNSUPPORTED);

  assert(unlink(path) == 0);
  assert(rmdir(directory) == 0);
  assert(rmdir(root) == 0);
  assert(unsetenv("TERMINFO") == 0);
}

static void
test_policy_and_output(int slave)
{
  int pipes[2];
  char bytes[64] = { 0 };
  ssize_t count;
  const char *styled, *cursor;
  int64_t caps;
  const char invalid_utf8[] = { (char)0xff, '\0' };

  test_compiled_terminfo(slave);
  assert(setenv("TERM", "xterm-on1x-definitely-missing", 1) == 0);
  assert(setenv("COLORTERM", "truecolor", 1) == 0);
  assert(unsetenv("NO_COLOR") == 0);
  caps = on1x_tty_capabilities(slave);
  assert(on1x_tty_status() == ON1X_TTY_OK);
  assert((caps & (ON1X_TTY_CAP_TTY | ON1X_TTY_CAP_ESCAPES
                  | ON1X_TTY_CAP_COLOR | ON1X_TTY_CAP_TRUECOLOR
                  | ON1X_TTY_CAP_ANSI_FALLBACK))
         == (ON1X_TTY_CAP_TTY | ON1X_TTY_CAP_ESCAPES | ON1X_TTY_CAP_COLOR
             | ON1X_TTY_CAP_TRUECOLOR | ON1X_TTY_CAP_ANSI_FALLBACK));
  styled = on1x_tty_render(slave, "safe\033[31m", MULIB_TERM_BOLD,
                          INT64_C(0x01abcdef), -1, true);
  assert(on1x_tty_status() == ON1X_TTY_OK);
  assert(strchr(styled, '\033') && strstr(styled, "safe"));
  assert(!strstr(styled, "\033[31m")); /* Source Escape was sanitized. */
  cursor = on1x_tty_control(slave, ON1X_TTY_CONTROL_CURSOR, 2, 4);
  assert(on1x_tty_status() == ON1X_TTY_OK);
  assert(!strcmp(cursor, "\033[3;5H"));
  assert(!strcmp(on1x_tty_control(slave, ON1X_TTY_CONTROL_PASTE_ENTER, 0, 0),
                 "\033[?2004h"));

  assert(setenv("NO_COLOR", "", 1) == 0); /* Presence is authoritative. */
  caps = on1x_tty_capabilities(slave);
  assert((caps & ON1X_TTY_CAP_NO_COLOR) && !(caps & ON1X_TTY_CAP_COLOR));
  assert(!strcmp(on1x_tty_render(slave, "plain", MULIB_TERM_BOLD, 1, -1, true),
                 "plain"));
  assert(on1x_tty_status() == ON1X_TTY_PLAIN);
  assert(!strcmp(on1x_tty_control(slave, ON1X_TTY_CONTROL_CURSOR, 0, 0),
                 "\033[1;1H")); /* NO_COLOR does not disable cursor control. */
  assert(!strcmp(on1x_tty_control(slave, ON1X_TTY_CONTROL_RESET, 0, 0), ""));
  assert(on1x_tty_status() == ON1X_TTY_PLAIN);

  assert(unsetenv("NO_COLOR") == 0);
  assert(setenv("TERM", "dumb", 1) == 0);
  assert(!strcmp(on1x_tty_control(slave, ON1X_TTY_CONTROL_CLEAR, 0, 0), ""));
  assert(on1x_tty_status() == ON1X_TTY_PLAIN);
  assert(setenv("TERM", "vt-on1x-unknown", 1) == 0);
  caps = on1x_tty_capabilities(slave);
  assert(caps == ON1X_TTY_CAP_TTY);
  assert(on1x_tty_status() == ON1X_TTY_OK);

  assert(pipe(pipes) == 0);
  caps = on1x_tty_capabilities(pipes[1]);
  assert(caps == 0 && on1x_tty_status() == ON1X_TTY_OK);
  assert(!strcmp(on1x_tty_render(pipes[1], "x", 256, -1, -1, false), ""));
  assert(on1x_tty_status() == ON1X_TTY_INVALID);
  assert(!strcmp(on1x_tty_render(pipes[1], invalid_utf8, 0, -1, -1, false),
                 ""));
  assert(on1x_tty_status() == ON1X_TTY_INVALID);
  assert(on1x_tty_write(pipes[1], "a\033[31mb", MULIB_TERM_BOLD, 1, -1, true)
         == 9);
  assert(on1x_tty_status() == ON1X_TTY_PLAIN);
  assert(close(pipes[1]) == 0);
  count = read(pipes[0], bytes, sizeof(bytes));
  assert(count == 9);
  assert(!memcmp(bytes, "a\xef\xbf\xbd[31mb", 9));
  assert(close(pipes[0]) == 0);
  assert(on1x_tty_capabilities(pipes[1]) == 0);
  assert(on1x_tty_status() == ON1X_TTY_INVALID);
  assert(!strcmp(styled, "\033[0m\033[39;49m\033[1m"
                         "\033[38;2;171;205;239msafe\xef\xbf\xbd[31m"
                         "\033[0m\033[39;49m"));
}

static void
test_read_hex(void)
{
  int pipes[2];
  static const unsigned char payload[] = { 'A', 0, 255 };
  assert(pipe(pipes) == 0);
  assert(write(pipes[1], payload, sizeof(payload)) == (ssize_t)sizeof(payload));
  assert(close(pipes[1]) == 0);
  assert(!strcmp(on1x_tty_read_hex(pipes[0], 16), "4100ff"));
  assert(on1x_tty_status() == ON1X_TTY_OK);
  assert(!strcmp(on1x_tty_read_hex(pipes[0], 16), ""));
  assert(on1x_tty_status() == ON1X_TTY_EOF);
  assert(close(pipes[0]) == 0);
  assert(!strcmp(on1x_tty_read_hex(-1, 16), ""));
  assert(on1x_tty_status() == ON1X_TTY_INVALID);
}

static void
test_raw_and_size(int slave)
{
  struct termios before, raw, after;
  struct winsize size = { 24, 80, 0, 0 };
  int64_t handle;
  int duplicate = dup(slave);
  assert(duplicate >= 0);
  assert(tcgetattr(slave, &before) == 0);
  assert(ioctl(slave, TIOCSWINSZ, &size) == 0);
  assert(on1x_tty_size(slave) == INT64_C(80) * 65536 + 24);
  assert(on1x_tty_status() == ON1X_TTY_OK);
  handle = on1x_tty_raw_begin(slave);
  assert(handle > 0 && on1x_tty_status() == ON1X_TTY_OK);
  assert(tcgetattr(slave, &raw) == 0);
  assert(!(raw.c_lflag & (ICANON | ECHO | ISIG)));
  assert(on1x_tty_raw_begin(slave) == -1);
  assert(on1x_tty_status() == ON1X_TTY_STATE);
  assert(on1x_tty_raw_begin(duplicate) == -1);
  assert(on1x_tty_status() == ON1X_TTY_STATE);
  assert(on1x_tty_raw_end(handle));
  assert(tcgetattr(slave, &after) == 0);
  assert_restored(&before, &after);
  assert(!on1x_tty_raw_end(handle));
  assert(on1x_tty_status() == ON1X_TTY_INVALID);
  assert(close(duplicate) == 0);
}

static void
test_exit_restore(int slave)
{
  struct termios before, after;
  pid_t child;
  int code;
  assert(tcgetattr(slave, &before) == 0);
  child = fork();
  assert(child >= 0);
  if (!child)
    {
      assert(on1x_tty_raw_begin(slave) > 0);
      exit(0); /* Adapter atexit cleanup must restore the borrowed terminal. */
    }
  assert(waitpid(child, &code, 0) == child);
  assert(WIFEXITED(code) && WEXITSTATUS(code) == 0);
  assert(tcgetattr(slave, &after) == 0);
  assert_restored(&before, &after);
}
#endif

int
main(void)
{
  test_decoder();
  test_decoder_limit();
  test_vterm();
#if defined(__unix__) || defined(__APPLE__)
  {
    int master, slave = open_terminal(&master);
    test_policy_and_output(slave);
    test_read_hex();
    test_raw_and_size(slave);
    test_exit_restore(slave);
    assert(close(slave) == 0);
    assert(close(master) == 0);
  }
#endif
  return 0;
}
