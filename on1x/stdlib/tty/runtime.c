/* Raw scalar companion. The canonical MuLIB planner owns every implementation
 * switch; this translation unit only calls public kernel declarations. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "runtime.h"

#include <mulib-termkey.h>
#include <mulib-terminfo.h>
#include <mulib-termstyle.h>
#include <mulib-termutils.h>
#include <mulib-vterm.h>

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifndef ON1X_TTY_MALLOC
#define ON1X_TTY_MALLOC malloc
#endif
#ifndef ON1X_TTY_FREE
#define ON1X_TTY_FREE free
#endif
#ifndef ON1X_TTY_POOL_BYTES
#define ON1X_TTY_POOL_BYTES (16u * 1024u * 1024u)
#endif

#define ON1X_TTY_TEXT_BYTES (1024u * 1024u)
#define ON1X_TTY_HEX_BYTES (2u * ON1X_TTY_TEXT_BYTES)
#define ON1X_TTY_SNAPSHOT_BYTES (2u * 1024u * 1024u)
#define ON1X_TTY_READ_BYTES 4096u
#define ON1X_TTY_HANDLE_FACTOR UINT64_C(256)
#define ON1X_TTY_RAW_SLOTS 16u
#define ON1X_TTY_DECODER_SLOTS 32u
#define ON1X_TTY_VTERM_SLOTS 16u
#define ON1X_TTY_DECODER_BASE 32u
#define ON1X_TTY_VTERM_BASE 64u

struct on1x_tty_saved {
  struct on1x_tty_saved *next;
  char value[];
};

struct on1x_tty_raw_slot {
  mulib_termutils_raw_t raw;
  uint64_t generation;
  int fd;
  bool active;
#if defined(__unix__) || defined(__APPLE__)
  dev_t device;
  ino_t inode;
  bool identity_valid;
#endif
};

struct on1x_tty_decoder_slot {
  mulib_termkey_t decoder;
  mulib_termkey_event_t event;
  uint64_t generation;
  bool active;
  bool event_valid;
};

struct on1x_tty_vterm_slot {
  mulib_vterm_t screen;
  uint64_t generation;
  bool active;
};

struct on1x_tty_profile {
  mulib_terminfo_t info;
  bool terminal;
  bool escapes;
  bool color;
  bool truecolor;
  bool loaded;
  bool fallback;
  bool no_color;
};

static int64_t on1x_tty_last_status;
static struct on1x_tty_saved *on1x_tty_saved_values;
static size_t on1x_tty_saved_bytes;
static bool on1x_tty_cleanup_registered;
static struct on1x_tty_raw_slot on1x_tty_raw_slots[ON1X_TTY_RAW_SLOTS];
static struct on1x_tty_decoder_slot
    on1x_tty_decoder_slots[ON1X_TTY_DECODER_SLOTS];
static struct on1x_tty_vterm_slot on1x_tty_vterm_slots[ON1X_TTY_VTERM_SLOTS];

int64_t
on1x_tty_status(void)
{
  return on1x_tty_last_status;
}

static void
on1x_tty_cleanup(void)
{
  size_t i;
  for (i = 0; i < ON1X_TTY_RAW_SLOTS; ++i)
    if (on1x_tty_raw_slots[i].active)
      {
        (void)mulib_termutils_raw_end(&on1x_tty_raw_slots[i].raw);
        on1x_tty_raw_slots[i].active = false;
      }
  for (i = 0; i < ON1X_TTY_VTERM_SLOTS; ++i)
    if (on1x_tty_vterm_slots[i].active)
      {
        mulib_vterm_free(&on1x_tty_vterm_slots[i].screen);
        on1x_tty_vterm_slots[i].active = false;
      }
  while (on1x_tty_saved_values)
    {
      struct on1x_tty_saved *saved = on1x_tty_saved_values;
      on1x_tty_saved_values = saved->next;
      ON1X_TTY_FREE(saved);
    }
  on1x_tty_saved_bytes = 0;
}

static bool
on1x_tty_register_cleanup(void)
{
  if (on1x_tty_cleanup_registered)
    return true;
  if (atexit(on1x_tty_cleanup))
    {
      on1x_tty_last_status = ON1X_TTY_MEMORY;
      return false;
    }
  on1x_tty_cleanup_registered = true;
  return true;
}

static bool
on1x_tty_bounded_length(const char *text, size_t limit, size_t *length)
{
  size_t i;
  if (!text || !length)
    return false;
  for (i = 0; i <= limit; ++i)
    if (!text[i])
      {
        *length = i;
        return true;
      }
  return false;
}

static char *
on1x_tty_reserve(size_t length)
{
  struct on1x_tty_saved *saved;
  size_t cost;
  if (length > ON1X_TTY_SNAPSHOT_BYTES
      || length > SIZE_MAX - sizeof(*saved) - 1u)
    {
      on1x_tty_last_status = ON1X_TTY_LIMIT;
      return NULL;
    }
  cost = sizeof(*saved) + length + 1u;
  if (on1x_tty_saved_bytes > ON1X_TTY_POOL_BYTES
      || cost > ON1X_TTY_POOL_BYTES - on1x_tty_saved_bytes)
    {
      on1x_tty_last_status = ON1X_TTY_LIMIT;
      return NULL;
    }
  if (!on1x_tty_register_cleanup())
    return NULL;
  saved = (struct on1x_tty_saved *)ON1X_TTY_MALLOC(cost);
  if (!saved)
    {
      on1x_tty_last_status = ON1X_TTY_MEMORY;
      return NULL;
    }
  saved->next = on1x_tty_saved_values;
  on1x_tty_saved_values = saved;
  on1x_tty_saved_bytes += cost;
  saved->value[length] = '\0';
  return saved->value;
}

static const char *
on1x_tty_save(const void *bytes, size_t length)
{
  char *output;
  if (!length)
    return "";
  output = on1x_tty_reserve(length);
  if (!output)
    return "";
  memcpy(output, bytes, length);
  return output;
}

static const char *
on1x_tty_hex(const unsigned char *bytes, size_t length)
{
  static const char alphabet[] = "0123456789abcdef";
  char *output;
  size_t i;
  if (length > ON1X_TTY_SNAPSHOT_BYTES / 2u)
    {
      on1x_tty_last_status = ON1X_TTY_LIMIT;
      return "";
    }
  if (!length)
    return "";
  output = on1x_tty_reserve(length * 2u);
  if (!output)
    return "";
  for (i = 0; i < length; ++i)
    {
      output[i * 2u] = alphabet[bytes[i] >> 4];
      output[i * 2u + 1u] = alphabet[bytes[i] & 15u];
    }
  return output;
}

static int
on1x_tty_nibble(unsigned char value)
{
  if (value >= '0' && value <= '9')
    return value - '0';
  if (value >= 'a' && value <= 'f')
    return value - 'a' + 10;
  if (value >= 'A' && value <= 'F')
    return value - 'A' + 10;
  return -1;
}

static bool
on1x_tty_hex_length(const char *hex, size_t limit, size_t *byte_length)
{
  size_t length, i;
  if (!hex)
    {
      on1x_tty_last_status = ON1X_TTY_INVALID;
      return false;
    }
  if (!on1x_tty_bounded_length(hex, limit, &length))
    {
      on1x_tty_last_status = ON1X_TTY_LIMIT;
      return false;
    }
  if (length & 1u)
    {
      on1x_tty_last_status = ON1X_TTY_INVALID;
      return false;
    }
  for (i = 0; i < length; ++i)
    if (on1x_tty_nibble((unsigned char)hex[i]) < 0)
      {
        on1x_tty_last_status = ON1X_TTY_INVALID;
        return false;
      }
  *byte_length = length / 2u;
  return true;
}

static void
on1x_tty_decode_hex(const char *hex, unsigned char *bytes, size_t length)
{
  size_t i;
  for (i = 0; i < length; ++i)
    bytes[i] = (unsigned char)((on1x_tty_nibble((unsigned char)hex[i * 2u])
                               << 4)
                              | on1x_tty_nibble(
                                  (unsigned char)hex[i * 2u + 1u]));
}

static bool
on1x_tty_fd(int64_t value, int *fd)
{
  if (value < 0 || value > INT_MAX)
    {
      on1x_tty_last_status = ON1X_TTY_INVALID;
      return false;
    }
  *fd = (int)value;
  return true;
}

static bool
on1x_tty_generation_available(uint64_t generation, unsigned code)
{
  return generation
         <= ((uint64_t)INT64_MAX - (uint64_t)code - 1u)
                / ON1X_TTY_HANDLE_FACTOR;
}

static int64_t
on1x_tty_handle(uint64_t generation, unsigned code)
{
  return (int64_t)(generation * ON1X_TTY_HANDLE_FACTOR + code + 1u);
}

static bool
on1x_tty_decode_handle(int64_t handle, unsigned first, unsigned count,
                       unsigned *index, uint64_t *generation)
{
  uint64_t raw, code;
  if (handle <= 0)
    return false;
  raw = (uint64_t)handle - 1u;
  code = raw % ON1X_TTY_HANDLE_FACTOR;
  *generation = raw / ON1X_TTY_HANDLE_FACTOR;
  if (code < first || code >= (uint64_t)first + count || !*generation)
    return false;
  *index = (unsigned)code - first;
  return true;
}

#if defined(__unix__) || defined(__APPLE__)
static bool
on1x_tty_term_name(const char *name, size_t *length)
{
  size_t i;
  if (!on1x_tty_bounded_length(name, 255u, length) || !*length
      || name[0] == '.')
    return false;
  for (i = 0; i < *length; ++i)
    {
      unsigned char c = (unsigned char)name[i];
      if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.'
            || c == '+'))
        return false;
    }
  return true;
}

static bool
on1x_tty_prefix(const char *text, const char *prefix)
{
  size_t length = strlen(prefix);
  return !strncmp(text, prefix, length);
}

static bool
on1x_tty_ansi_terminal(const char *name)
{
  static const char *const prefixes[] = {
    "xterm", "screen", "tmux", "rxvt", "ansi", "linux-", "cygwin-",
    "st-", "foot", "alacritty", "kitty", "wezterm", "konsole", "gnome",
    "eterm", "contour"
  };
  size_t i;
  if (!strcmp(name, "linux") || !strcmp(name, "cygwin") || !strcmp(name, "st")
      || !strcmp(name, "vt100") || !strcmp(name, "vt102")
      || !strcmp(name, "vt220") || !strcmp(name, "vt320")
      || !strcmp(name, "vt420") || !strcmp(name, "vt510")
      || !strcmp(name, "vt520") || !strcmp(name, "vt525"))
    return true;
  for (i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); ++i)
    if (on1x_tty_prefix(name, prefixes[i]))
      return true;
  return false;
}

static bool
on1x_tty_ascii_equal(const char *left, const char *right)
{
  while (*left && *right)
    {
      unsigned char a = (unsigned char)*left++;
      unsigned char b = (unsigned char)*right++;
      if (a >= 'A' && a <= 'Z')
        a = (unsigned char)(a + ('a' - 'A'));
      if (b >= 'A' && b <= 'Z')
        b = (unsigned char)(b + ('a' - 'A'));
      if (a != b)
        return false;
    }
  return !*left && !*right;
}

static bool
on1x_tty_try_dir(mulib_terminfo_t *info, const char *directory,
                 const char *terminal)
{
  size_t length;
  return on1x_tty_bounded_length(directory, 4095u, &length) && length
         && mulib_terminfo_load_dir(info, directory, terminal);
}

static bool
on1x_tty_load_terminfo(mulib_terminfo_t *info, const char *terminal)
{
  static const char *const defaults[] = {
    "/etc/terminfo", "/lib/terminfo", "/usr/share/terminfo",
    "/usr/local/share/terminfo"
  };
  const char *value;
  size_t length, i;
  char path[4096];

  value = getenv("TERMINFO");
  if (value && on1x_tty_try_dir(info, value, terminal))
    return true;

  value = getenv("HOME");
  if (value && on1x_tty_bounded_length(value, 4079u, &length)
      && length + sizeof("/.terminfo") <= sizeof(path))
    {
      memcpy(path, value, length);
      memcpy(path + length, "/.terminfo", sizeof("/.terminfo"));
      if (on1x_tty_try_dir(info, path, terminal))
        return true;
    }

  value = getenv("TERMINFO_DIRS");
  if (value && on1x_tty_bounded_length(value, 16384u, &length))
    {
      size_t begin = 0, directories = 0;
      for (i = 0; i <= length && directories < 32u; ++i)
        if (i == length || value[i] == ':')
          {
            size_t part = i - begin;
            if (part && part < sizeof(path))
              {
                memcpy(path, value + begin, part);
                path[part] = '\0';
                if (on1x_tty_try_dir(info, path, terminal))
                  return true;
              }
            begin = i + 1u;
            ++directories;
          }
    }

  for (i = 0; i < sizeof(defaults) / sizeof(defaults[0]); ++i)
    if (on1x_tty_try_dir(info, defaults[i], terminal))
      return true;
  return false;
}
#endif

static bool
on1x_tty_profile_load(int fd, struct on1x_tty_profile *profile)
{
  memset(profile, 0, sizeof(*profile));
  mulib_terminfo_init(&profile->info);
  profile->no_color = getenv("NO_COLOR") != NULL;
#if defined(__unix__) || defined(__APPLE__)
  const char *terminal, *color_term;
  size_t length;
  errno = 0;
  if (!isatty(fd))
    {
      if (errno == EBADF)
        {
          on1x_tty_last_status = ON1X_TTY_INVALID;
          return false;
        }
      return true;
    }
  profile->terminal = true;
  terminal = getenv("TERM");
  if (!terminal || !on1x_tty_term_name(terminal, &length)
      || !strcmp(terminal, "dumb"))
    return true;
  if (on1x_tty_load_terminfo(&profile->info, terminal))
    profile->loaded = true;
  else if (on1x_tty_ansi_terminal(terminal))
    {
      mulib_terminfo_ansi(&profile->info);
      profile->fallback = true;
    }
  else
    return true;
  profile->escapes = true;
  profile->color
      = !profile->no_color && mulib_terminfo_num(&profile->info, "colors") > 0;
  color_term = getenv("COLORTERM");
  if (profile->color && profile->info.ansi && color_term
      && on1x_tty_bounded_length(color_term, 64u, &length))
    profile->truecolor = on1x_tty_ascii_equal(color_term, "truecolor")
                         || on1x_tty_ascii_equal(color_term, "24bit");
  return true;
#else
  (void)fd;
  return true;
#endif
}

static void
on1x_tty_profile_free(struct on1x_tty_profile *profile)
{
  mulib_terminfo_free(&profile->info);
}

int64_t
on1x_tty_capabilities(int64_t descriptor)
{
  struct on1x_tty_profile profile;
  int fd;
  int64_t result = 0;
  on1x_tty_last_status = ON1X_TTY_OK;
  if (!on1x_tty_fd(descriptor, &fd)
      || !on1x_tty_profile_load(fd, &profile))
    return 0;
  if (profile.terminal)
    result |= ON1X_TTY_CAP_TTY;
  if (profile.escapes)
    result |= ON1X_TTY_CAP_ESCAPES;
  if (profile.color)
    result |= ON1X_TTY_CAP_COLOR;
  if (profile.truecolor)
    result |= ON1X_TTY_CAP_TRUECOLOR;
  if (profile.loaded)
    result |= ON1X_TTY_CAP_TERMINFO;
  if (profile.fallback)
    result |= ON1X_TTY_CAP_ANSI_FALLBACK;
  if (profile.no_color)
    result |= ON1X_TTY_CAP_NO_COLOR;
  on1x_tty_profile_free(&profile);
  return result;
}

static bool
on1x_tty_color(int64_t value, uint32_t *output)
{
  if (value == -1)
    {
      *output = MULIB_TERM_COLOR_DEFAULT;
      return true;
    }
  if ((value >= 0 && value <= 255)
      || (value >= INT64_C(0x01000000) && value <= INT64_C(0x01ffffff)))
    {
      *output = (uint32_t)value;
      return true;
    }
  return false;
}

static bool
on1x_tty_render_into(int fd, const char *text, int64_t attributes,
                     int64_t foreground, int64_t background, bool truecolor,
                     mulib_strbuf_t *output, int64_t *render_status)
{
  struct on1x_tty_profile profile;
  mulib_term_style_t style;
  size_t length;
  unsigned flags = 0;
  int result;

  if (!on1x_tty_bounded_length(text, ON1X_TTY_TEXT_BYTES, &length))
    {
      on1x_tty_last_status = text ? ON1X_TTY_LIMIT : ON1X_TTY_INVALID;
      return false;
    }
  if (attributes < 0 || attributes > 255
      || !on1x_tty_color(foreground, &style.foreground)
      || !on1x_tty_color(background, &style.background))
    {
      on1x_tty_last_status = ON1X_TTY_INVALID;
      return false;
    }
  style.attributes = (unsigned)attributes;
  if (!on1x_tty_profile_load(fd, &profile))
    return false;

  if (!profile.escapes || profile.no_color)
    flags |= MULIB_TERMSTYLE_PLAIN;
  else if (truecolor && profile.truecolor)
    flags |= MULIB_TERMSTYLE_TRUECOLOR;

  result = mulib_termstyle_write(style, profile.escapes ? &profile.info : NULL,
                                 flags, text, length, output);
  *render_status = (flags & MULIB_TERMSTYLE_PLAIN) ? ON1X_TTY_PLAIN
                                                   : ON1X_TTY_OK;
  if (result == MULIB_TERMSTYLE_ENOTSUP && !(flags & MULIB_TERMSTYLE_PLAIN))
    {
      mulib_strbuf_free(output);
      mulib_strbuf_init(output);
      result = mulib_termstyle_write(style, NULL, MULIB_TERMSTYLE_PLAIN, text,
                                     length, output);
      *render_status = ON1X_TTY_PLAIN;
    }
  on1x_tty_profile_free(&profile);
  if (result == MULIB_TERMSTYLE_OK)
    return true;
  on1x_tty_last_status
      = result == MULIB_TERMSTYLE_ENOMEM ? ON1X_TTY_MEMORY
        : result == MULIB_TERMSTYLE_ENOTSUP ? ON1X_TTY_UNSUPPORTED
                                            : ON1X_TTY_INVALID;
  return false;
}

const char *
on1x_tty_render(int64_t descriptor, const char *text, int64_t attributes,
                int64_t foreground, int64_t background, bool truecolor)
{
  mulib_strbuf_t output;
  const char *result;
  int64_t render_status;
  int fd;
  on1x_tty_last_status = ON1X_TTY_OK;
  if (!on1x_tty_fd(descriptor, &fd))
    return "";
  mulib_strbuf_init(&output);
  if (!on1x_tty_render_into(fd, text, attributes, foreground, background,
                            truecolor, &output, &render_status))
    {
      mulib_strbuf_free(&output);
      return "";
    }
  result = on1x_tty_save(output.data, output.length);
  mulib_strbuf_free(&output);
  if (on1x_tty_last_status != ON1X_TTY_MEMORY
      && on1x_tty_last_status != ON1X_TTY_LIMIT)
    on1x_tty_last_status = render_status;
  return result;
}

static void
on1x_tty_termutils_status(int result)
{
  on1x_tty_last_status
      = result == MULIB_TERMUTILS_OK ? ON1X_TTY_OK
        : result == MULIB_TERMUTILS_EINVAL ? ON1X_TTY_INVALID
        : result == MULIB_TERMUTILS_EIO ? ON1X_TTY_IO
        : result == MULIB_TERMUTILS_ENOTSUP ? ON1X_TTY_UNSUPPORTED
        : result == MULIB_TERMUTILS_ESTATE ? ON1X_TTY_STATE
                                           : ON1X_TTY_MEMORY;
}

int64_t
on1x_tty_write(int64_t descriptor, const char *text, int64_t attributes,
               int64_t foreground, int64_t background, bool truecolor)
{
  mulib_strbuf_t output;
  size_t written = 0;
  int64_t render_status;
  int fd, result;
  on1x_tty_last_status = ON1X_TTY_OK;
  if (!on1x_tty_fd(descriptor, &fd))
    return -1;
  mulib_strbuf_init(&output);
  if (!on1x_tty_render_into(fd, text, attributes, foreground, background,
                            truecolor, &output, &render_status))
    {
      mulib_strbuf_free(&output);
      return -1;
    }
  result = mulib_termutils_write(fd, output.data, output.length, &written);
  mulib_strbuf_free(&output);
  on1x_tty_termutils_status(result);
  if (result == MULIB_TERMUTILS_OK)
    on1x_tty_last_status = render_status;
  return (int64_t)written;
}

const char *
on1x_tty_control(int64_t descriptor, int64_t operation, int64_t first,
                 int64_t second)
{
  static const char *const names[] = {
    "sgr0", "clear", "cup", "civis", "cnorm", "smcup",
    "rmcup", "smkx",  "rmkx", "BE",    "BD",    "el"
  };
  struct on1x_tty_profile profile;
  mulib_strbuf_t output;
  int32_t params[9] = { 0 };
  const char *saved;
  int fd;

  on1x_tty_last_status = ON1X_TTY_OK;
  if (!on1x_tty_fd(descriptor, &fd) || operation < ON1X_TTY_CONTROL_RESET
      || operation > ON1X_TTY_CONTROL_ERASE_LINE)
    {
      if (on1x_tty_last_status == ON1X_TTY_OK)
        on1x_tty_last_status = ON1X_TTY_INVALID;
      return "";
    }
  if (operation == ON1X_TTY_CONTROL_CURSOR
      && (first < 0 || second < 0 || first >= INT32_MAX
          || second >= INT32_MAX))
    {
      on1x_tty_last_status = ON1X_TTY_INVALID;
      return "";
    }
  if (!on1x_tty_profile_load(fd, &profile))
    return "";
  if (!profile.escapes
      || (profile.no_color && operation == ON1X_TTY_CONTROL_RESET))
    {
      on1x_tty_profile_free(&profile);
      on1x_tty_last_status = ON1X_TTY_PLAIN;
      return "";
    }
  if (operation == ON1X_TTY_CONTROL_CURSOR)
    {
      params[0] = (int32_t)first;
      params[1] = (int32_t)second;
    }
  mulib_strbuf_init(&output);
  if (!mulib_terminfo_emit(&profile.info, names[operation], params, &output))
    {
      mulib_strbuf_free(&output);
      on1x_tty_profile_free(&profile);
      on1x_tty_last_status = ON1X_TTY_UNSUPPORTED;
      return "";
    }
  saved = on1x_tty_save(output.data, output.length);
  mulib_strbuf_free(&output);
  on1x_tty_profile_free(&profile);
  if (on1x_tty_last_status != ON1X_TTY_MEMORY
      && on1x_tty_last_status != ON1X_TTY_LIMIT)
    on1x_tty_last_status = ON1X_TTY_OK;
  return saved;
}

int64_t
on1x_tty_size(int64_t descriptor)
{
  unsigned columns, rows;
  int fd, result;
  on1x_tty_last_status = ON1X_TTY_OK;
  if (!on1x_tty_fd(descriptor, &fd))
    return -1;
  result = mulib_termutils_size(fd, &columns, &rows);
  on1x_tty_termutils_status(result);
  if (result != MULIB_TERMUTILS_OK)
    return -1;
  if (columns > 65535u || rows > 65535u)
    {
      on1x_tty_last_status = ON1X_TTY_LIMIT;
      return -1;
    }
  return (int64_t)columns * INT64_C(65536) + rows;
}

static struct on1x_tty_raw_slot *
on1x_tty_raw_slot(int64_t handle)
{
  unsigned index;
  uint64_t generation;
  if (on1x_tty_decode_handle(handle, 0, ON1X_TTY_RAW_SLOTS, &index,
                             &generation)
      && on1x_tty_raw_slots[index].active
      && on1x_tty_raw_slots[index].generation == generation)
    return &on1x_tty_raw_slots[index];
  on1x_tty_last_status = ON1X_TTY_INVALID;
  return NULL;
}

int64_t
on1x_tty_raw_begin(int64_t descriptor)
{
  size_t i;
  int fd, result;
#if defined(__unix__) || defined(__APPLE__)
  struct stat identity;
#endif
  on1x_tty_last_status = ON1X_TTY_OK;
  if (!on1x_tty_fd(descriptor, &fd))
    return -1;
#if defined(__unix__) || defined(__APPLE__)
  if (fstat(fd, &identity) < 0)
    {
      on1x_tty_last_status = errno == EBADF ? ON1X_TTY_INVALID : ON1X_TTY_IO;
      return -1;
    }
#endif
  for (i = 0; i < ON1X_TTY_RAW_SLOTS; ++i)
    if (on1x_tty_raw_slots[i].active
        && (on1x_tty_raw_slots[i].fd == fd
#if defined(__unix__) || defined(__APPLE__)
            || (on1x_tty_raw_slots[i].identity_valid
                && on1x_tty_raw_slots[i].device == identity.st_dev
                && on1x_tty_raw_slots[i].inode == identity.st_ino)
#endif
                ))
      {
        on1x_tty_last_status = ON1X_TTY_STATE;
        return -1;
      }
  for (i = 0; i < ON1X_TTY_RAW_SLOTS; ++i)
    if (!on1x_tty_raw_slots[i].active)
      {
        struct on1x_tty_raw_slot *slot = &on1x_tty_raw_slots[i];
        if (!slot->generation)
          slot->generation = 1;
        if (!on1x_tty_generation_available(slot->generation, (unsigned)i))
          continue;
        if (!on1x_tty_register_cleanup())
          return -1;
        slot->raw = (mulib_termutils_raw_t)MULIB_TERMUTILS_RAW_INIT;
        result = mulib_termutils_raw_begin(&slot->raw, fd);
        on1x_tty_termutils_status(result);
        if (result != MULIB_TERMUTILS_OK)
          return -1;
        slot->fd = fd;
        slot->active = true;
#if defined(__unix__) || defined(__APPLE__)
        slot->device = identity.st_dev;
        slot->inode = identity.st_ino;
        slot->identity_valid = true;
#endif
        return on1x_tty_handle(slot->generation, (unsigned)i);
      }
  on1x_tty_last_status = ON1X_TTY_LIMIT;
  return -1;
}

bool
on1x_tty_raw_end(int64_t handle)
{
  struct on1x_tty_raw_slot *slot;
  int result;
  on1x_tty_last_status = ON1X_TTY_OK;
  slot = on1x_tty_raw_slot(handle);
  if (!slot)
    return false;
  result = mulib_termutils_raw_end(&slot->raw);
  on1x_tty_termutils_status(result);
  if (result != MULIB_TERMUTILS_OK)
    return false;
  slot->active = false;
  slot->fd = -1;
#if defined(__unix__) || defined(__APPLE__)
  slot->identity_valid = false;
#endif
  ++slot->generation;
  return true;
}

const char *
on1x_tty_read_hex(int64_t descriptor, int64_t limit)
{
  unsigned char bytes[ON1X_TTY_READ_BYTES];
  int fd;
  on1x_tty_last_status = ON1X_TTY_OK;
  if (!on1x_tty_fd(descriptor, &fd) || limit < 1
      || limit > (int64_t)sizeof(bytes))
    {
      if (on1x_tty_last_status == ON1X_TTY_OK)
        on1x_tty_last_status = ON1X_TTY_INVALID;
      return "";
    }
#if defined(__unix__) || defined(__APPLE__)
  for (;;)
    {
      ssize_t count = read(fd, bytes, (size_t)limit);
      if (count < 0 && errno == EINTR)
        continue;
      if (count < 0)
        {
          on1x_tty_last_status
              = errno == EAGAIN || errno == EWOULDBLOCK ? ON1X_TTY_AGAIN
                                                       : ON1X_TTY_IO;
          return "";
        }
      if (!count)
        {
          on1x_tty_last_status = ON1X_TTY_EOF;
          return "";
        }
      return on1x_tty_hex(bytes, (size_t)count);
    }
#else
  (void)bytes;
  on1x_tty_last_status = ON1X_TTY_UNSUPPORTED;
  return "";
#endif
}

static struct on1x_tty_decoder_slot *
on1x_tty_decoder_slot(int64_t handle)
{
  unsigned index;
  uint64_t generation;
  if (on1x_tty_decode_handle(handle, ON1X_TTY_DECODER_BASE,
                             ON1X_TTY_DECODER_SLOTS, &index, &generation)
      && on1x_tty_decoder_slots[index].active
      && on1x_tty_decoder_slots[index].generation == generation)
    return &on1x_tty_decoder_slots[index];
  on1x_tty_last_status = ON1X_TTY_INVALID;
  return NULL;
}

int64_t
on1x_tty_decoder_open(void)
{
  size_t i;
  on1x_tty_last_status = ON1X_TTY_OK;
  for (i = 0; i < ON1X_TTY_DECODER_SLOTS; ++i)
    if (!on1x_tty_decoder_slots[i].active)
      {
        struct on1x_tty_decoder_slot *slot = &on1x_tty_decoder_slots[i];
        unsigned code = ON1X_TTY_DECODER_BASE + (unsigned)i;
        if (!slot->generation)
          slot->generation = 1;
        if (!on1x_tty_generation_available(slot->generation, code))
          continue;
        mulib_termkey_init(&slot->decoder);
        memset(&slot->event, 0, sizeof(slot->event));
        slot->event_valid = false;
        slot->active = true;
        return on1x_tty_handle(slot->generation, code);
      }
  on1x_tty_last_status = ON1X_TTY_LIMIT;
  return -1;
}

bool
on1x_tty_decoder_reset(int64_t handle)
{
  struct on1x_tty_decoder_slot *slot;
  on1x_tty_last_status = ON1X_TTY_OK;
  slot = on1x_tty_decoder_slot(handle);
  if (!slot)
    return false;
  mulib_termkey_init(&slot->decoder);
  slot->event_valid = false;
  return true;
}

bool
on1x_tty_decoder_feed(int64_t handle, const char *text)
{
  struct on1x_tty_decoder_slot *slot;
  size_t length;
  on1x_tty_last_status = ON1X_TTY_OK;
  slot = on1x_tty_decoder_slot(handle);
  if (!slot)
    return false;
  if (!on1x_tty_bounded_length(text, MULIB_TERMKEY_CAPACITY, &length))
    {
      on1x_tty_last_status = text ? ON1X_TTY_LIMIT : ON1X_TTY_INVALID;
      return false;
    }
  if (length > MULIB_TERMKEY_CAPACITY - slot->decoder.length)
    {
      on1x_tty_last_status = ON1X_TTY_LIMIT;
      return false;
    }
  if (mulib_termkey_feed(&slot->decoder, text, length) != length)
    {
      on1x_tty_last_status = ON1X_TTY_STATE;
      return false;
    }
  return true;
}

bool
on1x_tty_decoder_feed_hex(int64_t handle, const char *hex)
{
  struct on1x_tty_decoder_slot *slot;
  unsigned char bytes[MULIB_TERMKEY_CAPACITY];
  size_t length;
  on1x_tty_last_status = ON1X_TTY_OK;
  slot = on1x_tty_decoder_slot(handle);
  if (!slot)
    return false;
  if (!on1x_tty_hex_length(hex, MULIB_TERMKEY_CAPACITY * 2u, &length))
    return false;
  if (length > MULIB_TERMKEY_CAPACITY - slot->decoder.length)
    {
      on1x_tty_last_status = ON1X_TTY_LIMIT;
      return false;
    }
  on1x_tty_decode_hex(hex, bytes, length);
  if (mulib_termkey_feed(&slot->decoder, bytes, length) != length)
    {
      on1x_tty_last_status = ON1X_TTY_STATE;
      return false;
    }
  return true;
}

bool
on1x_tty_decoder_next(int64_t handle, bool flush)
{
  struct on1x_tty_decoder_slot *slot;
  int result;
  on1x_tty_last_status = ON1X_TTY_OK;
  slot = on1x_tty_decoder_slot(handle);
  if (!slot)
    return false;
  result = mulib_termkey_next(&slot->decoder, &slot->event, flush ? 1 : 0);
  slot->event_valid = result == 1;
  if (result == 1)
    return true;
  on1x_tty_last_status = result == 0 ? ON1X_TTY_AGAIN : ON1X_TTY_STATE;
  return false;
}

int64_t
on1x_tty_decoder_event_value(int64_t handle, int64_t field)
{
  struct on1x_tty_decoder_slot *slot;
  on1x_tty_last_status = ON1X_TTY_OK;
  slot = on1x_tty_decoder_slot(handle);
  if (!slot || !slot->event_valid || field < ON1X_TTY_EVENT_TYPE
      || field > ON1X_TTY_EVENT_LENGTH)
    {
      on1x_tty_last_status = ON1X_TTY_INVALID;
      return -1;
    }
  switch (field)
    {
    case ON1X_TTY_EVENT_TYPE:
      return slot->event.type;
    case ON1X_TTY_EVENT_KEY:
      return slot->event.key;
    case ON1X_TTY_EVENT_CODEPOINT:
      return slot->event.codepoint;
    case ON1X_TTY_EVENT_MODIFIERS:
      return slot->event.modifiers;
    case ON1X_TTY_EVENT_MOUSE_BUTTON:
      return slot->event.mouse_button;
    case ON1X_TTY_EVENT_MOUSE_ACTION:
      return slot->event.mouse_action;
    case ON1X_TTY_EVENT_X:
      return slot->event.x <= (size_t)INT64_MAX ? (int64_t)slot->event.x : -1;
    case ON1X_TTY_EVENT_Y:
      return slot->event.y <= (size_t)INT64_MAX ? (int64_t)slot->event.y : -1;
    default:
      return (int64_t)slot->event.length;
    }
}

const char *
on1x_tty_decoder_event_raw_hex(int64_t handle)
{
  struct on1x_tty_decoder_slot *slot;
  on1x_tty_last_status = ON1X_TTY_OK;
  slot = on1x_tty_decoder_slot(handle);
  if (!slot || !slot->event_valid)
    {
      on1x_tty_last_status = ON1X_TTY_INVALID;
      return "";
    }
  return on1x_tty_hex(slot->event.raw, slot->event.length);
}

bool
on1x_tty_decoder_close(int64_t handle)
{
  struct on1x_tty_decoder_slot *slot;
  on1x_tty_last_status = ON1X_TTY_OK;
  slot = on1x_tty_decoder_slot(handle);
  if (!slot)
    return false;
  memset(&slot->decoder, 0, sizeof(slot->decoder));
  memset(&slot->event, 0, sizeof(slot->event));
  slot->event_valid = false;
  slot->active = false;
  ++slot->generation;
  return true;
}

static bool
on1x_tty_vterm_dimensions(int64_t width, int64_t height)
{
  return width > 0 && height > 0 && width <= INT_MAX && height <= INT_MAX
         && (uint64_t)width <= MULIB_VTERM_MAX_CELLS
         && (uint64_t)height <= MULIB_VTERM_MAX_CELLS / (uint64_t)width;
}

static struct on1x_tty_vterm_slot *
on1x_tty_vterm_slot(int64_t handle)
{
  unsigned index;
  uint64_t generation;
  if (on1x_tty_decode_handle(handle, ON1X_TTY_VTERM_BASE,
                             ON1X_TTY_VTERM_SLOTS, &index, &generation)
      && on1x_tty_vterm_slots[index].active
      && on1x_tty_vterm_slots[index].generation == generation)
    return &on1x_tty_vterm_slots[index];
  on1x_tty_last_status = ON1X_TTY_INVALID;
  return NULL;
}

int64_t
on1x_tty_vterm_open(int64_t width, int64_t height)
{
  size_t i;
  on1x_tty_last_status = ON1X_TTY_OK;
  if (!on1x_tty_vterm_dimensions(width, height))
    {
      on1x_tty_last_status = ON1X_TTY_INVALID;
      return -1;
    }
  for (i = 0; i < ON1X_TTY_VTERM_SLOTS; ++i)
    if (!on1x_tty_vterm_slots[i].active)
      {
        struct on1x_tty_vterm_slot *slot = &on1x_tty_vterm_slots[i];
        unsigned code = ON1X_TTY_VTERM_BASE + (unsigned)i;
        if (!slot->generation)
          slot->generation = 1;
        if (!on1x_tty_generation_available(slot->generation, code))
          continue;
        if (!on1x_tty_register_cleanup())
          return -1;
        memset(&slot->screen, 0, sizeof(slot->screen));
        if (!mulib_vterm_init(&slot->screen, (size_t)width, (size_t)height))
          {
            on1x_tty_last_status = ON1X_TTY_MEMORY;
            return -1;
          }
        slot->active = true;
        return on1x_tty_handle(slot->generation, code);
      }
  on1x_tty_last_status = ON1X_TTY_LIMIT;
  return -1;
}

bool
on1x_tty_vterm_resize(int64_t handle, int64_t width, int64_t height)
{
  struct on1x_tty_vterm_slot *slot;
  on1x_tty_last_status = ON1X_TTY_OK;
  slot = on1x_tty_vterm_slot(handle);
  if (!slot)
    return false;
  if (!on1x_tty_vterm_dimensions(width, height))
    {
      on1x_tty_last_status = ON1X_TTY_INVALID;
      return false;
    }
  if (!mulib_vterm_resize(&slot->screen, (size_t)width, (size_t)height))
    {
      on1x_tty_last_status = ON1X_TTY_MEMORY;
      return false;
    }
  return true;
}

bool
on1x_tty_vterm_reset(int64_t handle)
{
  struct on1x_tty_vterm_slot *slot;
  on1x_tty_last_status = ON1X_TTY_OK;
  slot = on1x_tty_vterm_slot(handle);
  if (!slot)
    return false;
  mulib_vterm_reset(&slot->screen);
  return true;
}

bool
on1x_tty_vterm_feed(int64_t handle, const char *text)
{
  struct on1x_tty_vterm_slot *slot;
  size_t length;
  on1x_tty_last_status = ON1X_TTY_OK;
  slot = on1x_tty_vterm_slot(handle);
  if (!slot)
    return false;
  if (!on1x_tty_bounded_length(text, ON1X_TTY_TEXT_BYTES, &length))
    {
      on1x_tty_last_status = text ? ON1X_TTY_LIMIT : ON1X_TTY_INVALID;
      return false;
    }
  if (!mulib_vterm_feed(&slot->screen, text, length))
    {
      on1x_tty_last_status = ON1X_TTY_STATE;
      return false;
    }
  return true;
}

bool
on1x_tty_vterm_feed_hex(int64_t handle, const char *hex)
{
  struct on1x_tty_vterm_slot *slot;
  unsigned char bytes[512];
  size_t length, offset = 0;
  on1x_tty_last_status = ON1X_TTY_OK;
  slot = on1x_tty_vterm_slot(handle);
  if (!slot)
    return false;
  if (!on1x_tty_hex_length(hex, ON1X_TTY_HEX_BYTES, &length))
    return false;
  while (offset < length)
    {
      size_t count = length - offset;
      if (count > sizeof(bytes))
        count = sizeof(bytes);
      on1x_tty_decode_hex(hex + offset * 2u, bytes, count);
      if (!mulib_vterm_feed(&slot->screen, bytes, count))
        {
          on1x_tty_last_status = ON1X_TTY_STATE;
          return false;
        }
      offset += count;
    }
  return true;
}

bool
on1x_tty_vterm_finish(int64_t handle)
{
  struct on1x_tty_vterm_slot *slot;
  on1x_tty_last_status = ON1X_TTY_OK;
  slot = on1x_tty_vterm_slot(handle);
  if (!slot)
    return false;
  mulib_vterm_finish(&slot->screen);
  return true;
}

int64_t
on1x_tty_vterm_value(int64_t handle, int64_t field)
{
  struct on1x_tty_vterm_slot *slot;
  size_t value;
  on1x_tty_last_status = ON1X_TTY_OK;
  slot = on1x_tty_vterm_slot(handle);
  if (!slot || field < ON1X_TTY_VTERM_WIDTH
      || field > ON1X_TTY_VTERM_CURSOR_VISIBLE)
    {
      on1x_tty_last_status = ON1X_TTY_INVALID;
      return -1;
    }
  switch (field)
    {
    case ON1X_TTY_VTERM_WIDTH:
      value = slot->screen.grid.width;
      break;
    case ON1X_TTY_VTERM_HEIGHT:
      value = slot->screen.grid.height;
      break;
    case ON1X_TTY_VTERM_CURSOR_X:
      value = slot->screen.cursor_x;
      break;
    case ON1X_TTY_VTERM_CURSOR_Y:
      value = slot->screen.cursor_y;
      break;
    case ON1X_TTY_VTERM_INVALID_INPUT:
      value = slot->screen.invalid_input;
      break;
    case ON1X_TTY_VTERM_IGNORED_SEQUENCES:
      value = slot->screen.ignored_sequences;
      break;
    default:
      value = slot->screen.cursor_visible;
      break;
    }
  if (value > (size_t)INT64_MAX)
    {
      on1x_tty_last_status = ON1X_TTY_LIMIT;
      return -1;
    }
  return (int64_t)value;
}

int64_t
on1x_tty_vterm_cell(int64_t handle, int64_t x, int64_t y, int64_t field)
{
  struct on1x_tty_vterm_slot *slot;
  const mulib_term_cell_t *cell;
  on1x_tty_last_status = ON1X_TTY_OK;
  slot = on1x_tty_vterm_slot(handle);
  if (!slot || x < 0 || y < 0 || field < ON1X_TTY_CELL_CODEPOINT
      || field > ON1X_TTY_CELL_ATTRIBUTES)
    {
      on1x_tty_last_status = ON1X_TTY_INVALID;
      return -1;
    }
  cell = mulib_vterm_cell(&slot->screen, (size_t)x, (size_t)y);
  if (!cell)
    {
      on1x_tty_last_status = ON1X_TTY_INVALID;
      return -1;
    }
  if (field == ON1X_TTY_CELL_CODEPOINT)
    return cell->codepoint;
  if (field == ON1X_TTY_CELL_FOREGROUND)
    return cell->style.foreground;
  if (field == ON1X_TTY_CELL_BACKGROUND)
    return cell->style.background;
  return cell->style.attributes;
}

bool
on1x_tty_vterm_close(int64_t handle)
{
  struct on1x_tty_vterm_slot *slot;
  on1x_tty_last_status = ON1X_TTY_OK;
  slot = on1x_tty_vterm_slot(handle);
  if (!slot)
    return false;
  mulib_vterm_free(&slot->screen);
  slot->active = false;
  ++slot->generation;
  return true;
}
