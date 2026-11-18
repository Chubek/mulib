#ifndef ON1X_STDLIB_TTY_RUNTIME_H
#define ON1X_STDLIB_TTY_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Trusted raw scalar ABI. It is process-local and not thread-safe. Read status
 * immediately after an operation. Returned strings are immutable snapshots. */
enum on1x_tty_status_code {
  ON1X_TTY_OK = 0,
  ON1X_TTY_PLAIN = 1,
  ON1X_TTY_EOF = 2,
  ON1X_TTY_AGAIN = 3,
  ON1X_TTY_INVALID = -1,
  ON1X_TTY_IO = -2,
  ON1X_TTY_UNSUPPORTED = -3,
  ON1X_TTY_MEMORY = -4,
  ON1X_TTY_LIMIT = -5,
  ON1X_TTY_STATE = -6
};

enum on1x_tty_capability {
  ON1X_TTY_CAP_TTY = 1,
  ON1X_TTY_CAP_ESCAPES = 2,
  ON1X_TTY_CAP_COLOR = 4,
  ON1X_TTY_CAP_TRUECOLOR = 8,
  ON1X_TTY_CAP_TERMINFO = 16,
  ON1X_TTY_CAP_ANSI_FALLBACK = 32,
  ON1X_TTY_CAP_NO_COLOR = 64
};

enum on1x_tty_control {
  ON1X_TTY_CONTROL_RESET = 0,
  ON1X_TTY_CONTROL_CLEAR = 1,
  ON1X_TTY_CONTROL_CURSOR = 2,
  ON1X_TTY_CONTROL_CURSOR_HIDE = 3,
  ON1X_TTY_CONTROL_CURSOR_SHOW = 4,
  ON1X_TTY_CONTROL_ALT_ENTER = 5,
  ON1X_TTY_CONTROL_ALT_LEAVE = 6,
  ON1X_TTY_CONTROL_KEYPAD_ENTER = 7,
  ON1X_TTY_CONTROL_KEYPAD_LEAVE = 8,
  ON1X_TTY_CONTROL_PASTE_ENTER = 9,
  ON1X_TTY_CONTROL_PASTE_LEAVE = 10,
  ON1X_TTY_CONTROL_ERASE_LINE = 11
};

enum on1x_tty_event_field {
  ON1X_TTY_EVENT_TYPE = 0,
  ON1X_TTY_EVENT_KEY = 1,
  ON1X_TTY_EVENT_CODEPOINT = 2,
  ON1X_TTY_EVENT_MODIFIERS = 3,
  ON1X_TTY_EVENT_MOUSE_BUTTON = 4,
  ON1X_TTY_EVENT_MOUSE_ACTION = 5,
  ON1X_TTY_EVENT_X = 6,
  ON1X_TTY_EVENT_Y = 7,
  ON1X_TTY_EVENT_LENGTH = 8
};

enum on1x_tty_vterm_field {
  ON1X_TTY_VTERM_WIDTH = 0,
  ON1X_TTY_VTERM_HEIGHT = 1,
  ON1X_TTY_VTERM_CURSOR_X = 2,
  ON1X_TTY_VTERM_CURSOR_Y = 3,
  ON1X_TTY_VTERM_INVALID_INPUT = 4,
  ON1X_TTY_VTERM_IGNORED_SEQUENCES = 5,
  ON1X_TTY_VTERM_CURSOR_VISIBLE = 6
};

enum on1x_tty_cell_field {
  ON1X_TTY_CELL_CODEPOINT = 0,
  ON1X_TTY_CELL_FOREGROUND = 1,
  ON1X_TTY_CELL_BACKGROUND = 2,
  ON1X_TTY_CELL_ATTRIBUTES = 3
};

int64_t on1x_tty_status(void);

/* Terminal policy and safe output. render/write replace untrusted terminal
 * controls in text. Colors are -1 (default), 0..255, or 0x01RRGGBB. */
int64_t on1x_tty_capabilities(int64_t fd);
const char *on1x_tty_render(int64_t fd, const char *text, int64_t attributes,
                            int64_t foreground, int64_t background,
                            bool truecolor);
int64_t on1x_tty_write(int64_t fd, const char *text, int64_t attributes,
                       int64_t foreground, int64_t background, bool truecolor);
const char *on1x_tty_control(int64_t fd, int64_t operation, int64_t first,
                             int64_t second);
/* Success packs columns in bits 16..31 and rows in bits 0..15. */
int64_t on1x_tty_size(int64_t fd);

/* A raw handle owns saved terminal attributes but borrows the fd. */
int64_t on1x_tty_raw_begin(int64_t fd);
bool on1x_tty_raw_end(int64_t handle);

/* Blocking/nonblocking read according to fd configuration. The result is
 * lowercase hex so embedded NUL and invalid UTF-8 remain lossless. */
const char *on1x_tty_read_hex(int64_t fd, int64_t limit);

/* Incremental key/mouse/paste decoder. Feed is all-or-nothing and bounded by
 * the kernel's 128-byte pending capacity. Call next with flush after the
 * caller's Escape timeout or at end-of-stream. */
int64_t on1x_tty_decoder_open(void);
bool on1x_tty_decoder_reset(int64_t handle);
bool on1x_tty_decoder_feed(int64_t handle, const char *text);
bool on1x_tty_decoder_feed_hex(int64_t handle, const char *hex);
bool on1x_tty_decoder_next(int64_t handle, bool flush);
int64_t on1x_tty_decoder_event_value(int64_t handle, int64_t field);
const char *on1x_tty_decoder_event_raw_hex(int64_t handle);
bool on1x_tty_decoder_close(int64_t handle);

/* Bounded virtual terminal. It is a one-scalar-per-cell model, not full DEC
 * emulation; see mulib-vterm's contract for the supported protocol. */
int64_t on1x_tty_vterm_open(int64_t width, int64_t height);
bool on1x_tty_vterm_resize(int64_t handle, int64_t width, int64_t height);
bool on1x_tty_vterm_reset(int64_t handle);
bool on1x_tty_vterm_feed(int64_t handle, const char *text);
bool on1x_tty_vterm_feed_hex(int64_t handle, const char *hex);
bool on1x_tty_vterm_finish(int64_t handle);
int64_t on1x_tty_vterm_value(int64_t handle, int64_t field);
int64_t on1x_tty_vterm_cell(int64_t handle, int64_t x, int64_t y,
                            int64_t field);
bool on1x_tty_vterm_close(int64_t handle);

#ifdef __cplusplus
}
#endif
#endif
