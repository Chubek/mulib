/**
 * @file mulib-imtui-terminal.h
 * @brief Explicit POSIX terminal session for byte renderers and termkey input.
 *
 * Define MULIB_IMPL_IMTUI_TERMINAL once, and provide MULIB_IMPL_TERMUTILS,
 * MULIB_IMPL_TERMKEY, MULIB_IMPL_UTF8, MULIB_IMPL_TERMINFO and MULIB_IMPL_STRBUF.
 * No renderer, clock, environment lookup, signal handler or atexit hook.
 *
 * Use one owner per tty, in the foreground. Arrange close/abort on all normal
 * exit paths and restore before suspension (resume with a new session). Do
 * not call these functions from signal handlers. A crash, SIGKILL, a lost tty,
 * or an output error can prevent restoration. Protocol cleanup sends inverse
 * commands; it does not discover or preserve preexisting emulator modes.
 * When using pipes/sockets, the caller is responsible for SIGPIPE policy.
 */
#ifndef MULIB_IMTUI_TERMINAL_H
#define MULIB_IMTUI_TERMINAL_H
#include "mulib-terminfo.h"
#include "mulib-termkey.h"
#include "mulib-termutils.h"
#ifdef __cplusplus
extern "C"
{
#endif

  enum
  {
    MULIB_IMTUI_TERMINAL_OK = 0,
    MULIB_IMTUI_TERMINAL_EVENT = 1,
    MULIB_IMTUI_TERMINAL_AGAIN = 2,
    MULIB_IMTUI_TERMINAL_EOF = 3,
    MULIB_IMTUI_TERMINAL_EINVAL = -1,
    MULIB_IMTUI_TERMINAL_ESTATE = -2,
    MULIB_IMTUI_TERMINAL_EIO = -3, /**< errno describes the OS failure. */
    MULIB_IMTUI_TERMINAL_NOMEM = -4,
    MULIB_IMTUI_TERMINAL_ECAP = -5, /**< Could not expand a terminfo pair. */
    MULIB_IMTUI_TERMINAL_TOO_LARGE = -6
  };

  enum
  {
    MULIB_IMTUI_TERMINAL_RAW = 1u << 0,
    MULIB_IMTUI_TERMINAL_ALT_SCREEN = 1u << 1,
    MULIB_IMTUI_TERMINAL_HIDE_CURSOR = 1u << 2,
    MULIB_IMTUI_TERMINAL_MOUSE = 1u << 3, /**< xterm SGR button/drag reports. */
    MULIB_IMTUI_TERMINAL_BRACKETED_PASTE = 1u << 4,
    MULIB_IMTUI_TERMINAL_PLAIN = 1u << 5 /**< Suppress every other mode flag. */
  };

#define MULIB_IMTUI_TERMINAL_OUTPUT_LIMIT 65536u
  typedef struct mulib_imtui_terminal_options
  {
    unsigned flags;
    unsigned fallback_columns, fallback_rows; /**< Zero in either disables. */
    size_t output_limit; /**< Pending app bytes; zero selects default. */
    /**
     * Borrowed only during open/attach. NULL selects the ANSI profile.
     * For a supplied record, a missing enter/leave capability pair is skipped.
     * xterm mouse and paste modes are explicit opt-ins independent of terminfo.
     */
    const mulib_terminfo_t *terminfo;
  } mulib_imtui_terminal_options_t;

#define MULIB_IMTUI_TERMINAL_OPTIONS_INIT                                    \
  { MULIB_IMTUI_TERMINAL_RAW | MULIB_IMTUI_TERMINAL_ALT_SCREEN                 \
        | MULIB_IMTUI_TERMINAL_HIDE_CURSOR,                                   \
    80, 24, 0, NULL }

  /** @brief Owned session. Zero-initialize, never copy or edit members. */
  typedef struct mulib_imtui_terminal
  {
    int input_fd, output_fd;
    unsigned state, owned, eof, escape_wait;
    unsigned fallback_columns, fallback_rows;
    size_t output_limit, enter_offset, output_offset, leave_offset;
    mulib_termutils_raw_t raw;
    mulib_termkey_t decoder;
    mulib_strbuf_t enter, output, leave;
  } mulib_imtui_terminal_t;

  /**
   * @brief Open path as a private nonblocking tty (NULL means /dev/tty).
   * NULL options selects defaults. Open/attach enter raw mode if requested
   * and queue startup commands, but never write them. On failure the session
   * is unchanged. Call flush before displaying the first frame.
   */
  int mulib_imtui_terminal_open (mulib_imtui_terminal_t *session,
                                const char *path,
                                const mulib_imtui_terminal_options_t *options);
  /**
   * @brief Borrow input/output fds, which may be the same fd.
   * Both must be open and nonblocking until cleanup. No fd flags are modified.
   * With RAW off, non-ttys work (including injected pipes).
   * PLAIN does not filter application output or change termkey decoding.
   * When using RAW with different fds, they should refer to the same tty if
   * output post-processing must also be disabled.
   */
  int mulib_imtui_terminal_attach (
      mulib_imtui_terminal_t *session, int input_fd, int output_fd,
      const mulib_imtui_terminal_options_t *options);
  /** @brief Descriptors for poll; -1 when closed or session is NULL. */
  int mulib_imtui_terminal_input_fd (const mulib_imtui_terminal_t *session);
  int mulib_imtui_terminal_output_fd (const mulib_imtui_terminal_t *session);
  /** @brief Query live tty size (output first, then input) or configured
   * fallback. On failure, the outputs are unchanged. */
  int mulib_imtui_terminal_size (const mulib_imtui_terminal_t *session,
                                unsigned *columns, unsigned *rows);

  /**
   * @brief Copy an entire arbitrary byte span, without I/O.
   * AGAIN means flush and retry (no bytes accepted); TOO_LARGE means the span
   * exceeds the entire queue limit. Enter/leave sequences have separate space.
   * The caller is responsible for resetting any additional renderer modes.
   */
  int mulib_imtui_terminal_queue (mulib_imtui_terminal_t *session,
                                 const void *bytes, size_t length);
  /** @brief True if there are queued bytes; on AGAIN wait for writable fd. */
  int mulib_imtui_terminal_output_pending (
      const mulib_imtui_terminal_t *session);
  /** @brief Send queued startup and app bytes in order; preserves the
   * unwritten suffix on AGAIN or EIO. Invalid once close has begun. */
  int mulib_imtui_terminal_flush (mulib_imtui_terminal_t *session);

  /**
   * @brief Pull one value event, reading when needed. Drain until AGAIN/EOF.
   * EOF resolves incomplete input before being reported and remains sticky.
   * Only EVENT produces a valid event. On AGAIN, poll for input and, if
   * escape_pending is true, arm a caller-owned deadline. On expiry, call
   * flush_input before reading anything that should follow the expired Escape.
   */
  int mulib_imtui_terminal_next (mulib_imtui_terminal_t *session,
                                mulib_termkey_event_t *event);
  /** @brief After AGAIN, true for an incomplete ESC-prefixed key, not paste
   * delimiters or ordinary partial UTF-8. No timer is run by the backend. */
  int mulib_imtui_terminal_escape_pending (
      const mulib_imtui_terminal_t *session);
  /** @brief Force one buffered event without a read; EVENT or AGAIN when empty. */
  int mulib_imtui_terminal_flush_input (mulib_imtui_terminal_t *session,
                                       mulib_termkey_event_t *event);

  /**
   * @brief Flush startup/app bytes, emit cleanup, restore termios and release.
   * AGAIN leaves the session closing (including raw mode): poll for output
   * and retry close or use abort. Other returns finish teardown, even on I/O
   * error. Idempotent on a closed session.
   */
  int mulib_imtui_terminal_close (mulib_imtui_terminal_t *session);
  /**
   * @brief Drop unsent startup/app bytes, try cleanup without waiting if any
   * startup was written, then try to restore termios and release even on failure.
   * Returns EIO if best-effort cleanup was incomplete. Idempotent when closed.
   */
  int mulib_imtui_terminal_abort (mulib_imtui_terminal_t *session);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMTUI_TERMINAL_H */

#if defined(MULIB_IMPL_IMTUI_TERMINAL) && !defined(MULIB_IMTUI_TERMINAL_IMPLEMENTED)
#define MULIB_IMTUI_TERMINAL_IMPLEMENTED
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __cplusplus
extern "C"
{
#endif

  enum { mulib_imtui_terminal_active_ = 1, mulib_imtui_terminal_closing_ = 2 };

  static int
  mulib_imtui_terminal_pair_ (mulib_imtui_terminal_t *s,
                             const mulib_terminfo_t *info,
                             const char *enter, const char *leave,
                             mulib_strbuf_t *restore)
  {
    const char *a = mulib_terminfo_str (info, enter);
    const char *b = mulib_terminfo_str (info, leave);
    size_t enter_len = s->enter.length, restore_len = restore->length;
    if (a && *a && b && *b)
      {
        if (!mulib_terminfo_expand (a, NULL, &s->enter)
            || !mulib_terminfo_expand (b, NULL, restore))
          return 0;
        if (s->enter.length == enter_len || restore->length == restore_len)
          {
            s->enter.length = enter_len;
            restore->length = restore_len;
            if (s->enter.data)
              s->enter.data[enter_len] = '\0';
            if (restore->data)
              restore->data[restore_len] = '\0';
          }
      }
    return 1;
  }

  static int
  mulib_imtui_terminal_modes_ (mulib_imtui_terminal_t *s, unsigned flags,
                              const mulib_terminfo_t *info)
  {
    mulib_strbuf_t alternate, cursor;
    mulib_terminfo_t ansi;
    int rc = MULIB_IMTUI_TERMINAL_OK;
    mulib_strbuf_init (&alternate);
    mulib_strbuf_init (&cursor);
    if (!info)
      {
        mulib_terminfo_init (&ansi);
        mulib_terminfo_ansi (&ansi);
        info = &ansi;
      }
    if (((flags & MULIB_IMTUI_TERMINAL_ALT_SCREEN)
         && !mulib_imtui_terminal_pair_ (s, info, "smcup", "rmcup", &alternate))
        || ((flags & MULIB_IMTUI_TERMINAL_HIDE_CURSOR)
            && !mulib_imtui_terminal_pair_ (s, info, "civis", "cnorm", &cursor)))
      rc = MULIB_IMTUI_TERMINAL_ECAP;
    /* Select SGR coordinates before enabling button-event tracking; turn off
     * reporting before switching back to the legacy mouse encoding. */
    else if (((flags & MULIB_IMTUI_TERMINAL_MOUSE)
              && !mulib_strbuf_append (&s->enter, "\033[?1006h\033[?1002h"))
             || ((flags & MULIB_IMTUI_TERMINAL_BRACKETED_PASTE)
                 && (!mulib_strbuf_append (&s->enter, "\033[?2004h")
                     || !mulib_strbuf_append (&s->leave, "\033[?2004l")))
             || ((flags & MULIB_IMTUI_TERMINAL_MOUSE)
                 && !mulib_strbuf_append (&s->leave, "\033[?1002l\033[?1006l"))
             || (cursor.length
                 && !mulib_strbuf_append_n (&s->leave, cursor.data, cursor.length))
             || (alternate.length
                 && !mulib_strbuf_append_n (&s->leave, alternate.data, alternate.length)))
      rc = MULIB_IMTUI_TERMINAL_NOMEM;
    mulib_strbuf_free (&cursor);
    mulib_strbuf_free (&alternate);
    return rc;
  }

  static int
  mulib_imtui_terminal_nonblock_ (int fd, int forbidden)
  {
    int flags;
    do
      flags = fcntl (fd, F_GETFL);
    while (flags < 0 && errno == EINTR);
    if (flags < 0)
      return MULIB_IMTUI_TERMINAL_EIO;
    if (!(flags & O_NONBLOCK) || (flags & O_ACCMODE) == forbidden)
      return MULIB_IMTUI_TERMINAL_EINVAL;
    return MULIB_IMTUI_TERMINAL_OK;
  }

  int
  mulib_imtui_terminal_attach (mulib_imtui_terminal_t *s, int input_fd,
                              int output_fd,
                              const mulib_imtui_terminal_options_t *options)
  {
    const mulib_imtui_terminal_options_t defaults = MULIB_IMTUI_TERMINAL_OPTIONS_INIT;
    const unsigned modes = MULIB_IMTUI_TERMINAL_RAW | MULIB_IMTUI_TERMINAL_ALT_SCREEN
                           | MULIB_IMTUI_TERMINAL_HIDE_CURSOR | MULIB_IMTUI_TERMINAL_MOUSE
                           | MULIB_IMTUI_TERMINAL_BRACKETED_PASTE;
    mulib_imtui_terminal_t next;
    unsigned flags;
    int rc, error;
    if (!s || input_fd < 0 || output_fd < 0)
      return MULIB_IMTUI_TERMINAL_EINVAL;
    if (s->state)
      return MULIB_IMTUI_TERMINAL_ESTATE;
    if (!options)
      options = &defaults;
    flags = options->flags;
    if ((flags & ~(modes | MULIB_IMTUI_TERMINAL_PLAIN))
        || options->output_limit == SIZE_MAX)
      return MULIB_IMTUI_TERMINAL_EINVAL;
    rc = mulib_imtui_terminal_nonblock_ (input_fd, O_WRONLY);
    if (!rc)
      rc = mulib_imtui_terminal_nonblock_ (output_fd, O_RDONLY);
    if (rc)
      return rc;
    memset (&next, 0, sizeof (next));
    next.raw.fd = -1;
    next.input_fd = input_fd;
    next.output_fd = output_fd;
    next.fallback_columns = options->fallback_columns;
    next.fallback_rows = options->fallback_rows;
    next.output_limit = options->output_limit ? options->output_limit
                                              : MULIB_IMTUI_TERMINAL_OUTPUT_LIMIT;
    mulib_termkey_init (&next.decoder);
    if (!(flags & MULIB_IMTUI_TERMINAL_PLAIN))
      {
        rc = mulib_imtui_terminal_modes_ (&next, flags, options->terminfo);
        if (!rc && (flags & MULIB_IMTUI_TERMINAL_RAW))
          {
            rc = mulib_termutils_raw_begin (&next.raw, input_fd);
            if (rc)
              rc = rc == MULIB_TERMUTILS_NOMEM ? MULIB_IMTUI_TERMINAL_NOMEM
                                               : MULIB_IMTUI_TERMINAL_EIO;
          }
        if (rc)
          {
            error = errno;
            mulib_strbuf_free (&next.enter);
            mulib_strbuf_free (&next.leave);
            errno = error;
            return rc;
          }
      }
    next.state = mulib_imtui_terminal_active_;
    *s = next;
    return MULIB_IMTUI_TERMINAL_OK;
  }

  int
  mulib_imtui_terminal_open (mulib_imtui_terminal_t *s, const char *path,
                            const mulib_imtui_terminal_options_t *options)
  {
    int fd, rc = 0, error, flags = O_RDWR | O_NOCTTY | O_NONBLOCK;
    if (!s)
      return MULIB_IMTUI_TERMINAL_EINVAL;
    if (s->state)
      return MULIB_IMTUI_TERMINAL_ESTATE;
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif
    do
      fd = open (path ? path : "/dev/tty", flags);
    while (fd < 0 && errno == EINTR);
    if (fd < 0)
      return MULIB_IMTUI_TERMINAL_EIO;
#ifndef O_CLOEXEC
    do
      rc = fcntl (fd, F_SETFD, FD_CLOEXEC);
    while (rc < 0 && errno == EINTR);
#endif
    if (rc < 0 || !isatty (fd))
      rc = MULIB_IMTUI_TERMINAL_EIO;
    else
      rc = mulib_imtui_terminal_attach (s, fd, fd, options);
    if (rc)
      {
        error = errno;
        (void)close (fd);
        errno = error;
        return rc;
      }
    s->owned = 1;
    return MULIB_IMTUI_TERMINAL_OK;
  }

  int
  mulib_imtui_terminal_input_fd (const mulib_imtui_terminal_t *s)
  {
    return s && s->state ? s->input_fd : -1;
  }

  int
  mulib_imtui_terminal_output_fd (const mulib_imtui_terminal_t *s)
  {
    return s && s->state ? s->output_fd : -1;
  }

  int
  mulib_imtui_terminal_size (const mulib_imtui_terminal_t *s, unsigned *cols,
                            unsigned *rows)
  {
    unsigned c, r;
    int rc;
    if (!s || !cols || !rows || cols == rows)
      return MULIB_IMTUI_TERMINAL_EINVAL;
    if (s->state != mulib_imtui_terminal_active_)
      return MULIB_IMTUI_TERMINAL_ESTATE;
    rc = mulib_termutils_size (s->output_fd, &c, &r);
    if ((rc || !c || !r) && s->input_fd != s->output_fd)
      rc = mulib_termutils_size (s->input_fd, &c, &r);
    if (!rc && c && r)
      {
        *cols = c;
        *rows = r;
        return MULIB_IMTUI_TERMINAL_OK;
      }
    if (s->fallback_columns && s->fallback_rows)
      {
        *cols = s->fallback_columns;
        *rows = s->fallback_rows;
        return MULIB_IMTUI_TERMINAL_OK;
      }
    /* A tty with an unset window size may report success and zero dimensions. */
    if (!rc)
      errno = ENOTTY;
    return MULIB_IMTUI_TERMINAL_EIO;
  }

  int
  mulib_imtui_terminal_queue (mulib_imtui_terminal_t *s,
                             const void *bytes, size_t length)
  {
    size_t pending;
    if (!s || (!bytes && length))
      return MULIB_IMTUI_TERMINAL_EINVAL;
    if (s->state != mulib_imtui_terminal_active_)
      return MULIB_IMTUI_TERMINAL_ESTATE;
    if (length > s->output_limit)
      return MULIB_IMTUI_TERMINAL_TOO_LARGE;
    pending = s->output.length - s->output_offset;
    if (length > s->output_limit - pending)
      return MULIB_IMTUI_TERMINAL_AGAIN;
    if (!length)
      return MULIB_IMTUI_TERMINAL_OK;
    if (s->output_offset)
      {
        memmove (s->output.data, s->output.data + s->output_offset, pending);
        s->output.data[pending] = '\0';
        s->output.length = pending;
        s->output_offset = 0;
      }
    return mulib_strbuf_append_n (&s->output, (const char *)bytes, length)
               ? MULIB_IMTUI_TERMINAL_OK : MULIB_IMTUI_TERMINAL_NOMEM;
  }

  int
  mulib_imtui_terminal_output_pending (const mulib_imtui_terminal_t *s)
  {
    return s && s->state
           && (s->enter_offset < s->enter.length
               || s->output_offset < s->output.length
               || (s->state == mulib_imtui_terminal_closing_
                   && s->leave_offset < s->leave.length));
  }

  static int
  mulib_imtui_terminal_write_ (int fd, const mulib_strbuf_t *buf, size_t *offset)
  {
    size_t wrote;
    if (*offset == buf->length)
      return MULIB_IMTUI_TERMINAL_OK;
    if (mulib_termutils_write (fd, buf->data + *offset, buf->length - *offset,
                               &wrote) == 0)
      {
        *offset = buf->length;
        return MULIB_IMTUI_TERMINAL_OK;
      }
    *offset += wrote;
    return errno == EAGAIN || errno == EWOULDBLOCK ? MULIB_IMTUI_TERMINAL_AGAIN
                                                 : MULIB_IMTUI_TERMINAL_EIO;
  }

  static int
  mulib_imtui_terminal_drain_ (mulib_imtui_terminal_t *s)
  {
    int rc = mulib_imtui_terminal_write_ (s->output_fd, &s->enter, &s->enter_offset);
    if (!rc)
      rc = mulib_imtui_terminal_write_ (s->output_fd, &s->output, &s->output_offset);
    return rc;
  }

  int
  mulib_imtui_terminal_flush (mulib_imtui_terminal_t *s)
  {
    if (!s)
      return MULIB_IMTUI_TERMINAL_EINVAL;
    if (s->state != mulib_imtui_terminal_active_)
      return MULIB_IMTUI_TERMINAL_ESTATE;
    return mulib_imtui_terminal_drain_ (s);
  }

  int
  mulib_imtui_terminal_next (mulib_imtui_terminal_t *s, mulib_termkey_event_t *event)
  {
    unsigned char bytes[MULIB_TERMKEY_CAPACITY];
    ssize_t n;
    if (!s || !event)
      return MULIB_IMTUI_TERMINAL_EINVAL;
    if (s->state != mulib_imtui_terminal_active_)
      return MULIB_IMTUI_TERMINAL_ESTATE;
    s->escape_wait = 0;
    for (;;)
      {
        if (mulib_termkey_next (&s->decoder, event, s->eof != 0) == 1)
          return MULIB_IMTUI_TERMINAL_EVENT;
        if (s->eof)
          return MULIB_IMTUI_TERMINAL_EOF;
        if (s->decoder.length >= sizeof (bytes))
          {
            /* Defensive guard: termkey promises progress at full capacity. */
            errno = EIO;
            return MULIB_IMTUI_TERMINAL_EIO;
          }
        do
          n = read (s->input_fd, bytes, sizeof (bytes) - s->decoder.length);
        while (n < 0 && errno == EINTR);
        if (n > 0)
          (void)mulib_termkey_feed (&s->decoder, bytes, (size_t)n);
        else if (!n)
          {
            /* Noncanonical O_NONBLOCK tty reads may report zero without EOF. */
            if (s->raw.saved)
              {
                struct pollfd p = { s->input_fd, POLLIN, 0 };
                int rc;
                do
                  rc = poll (&p, 1, 0);
                while (rc < 0 && errno == EINTR);
                if (rc < 0)
                  return MULIB_IMTUI_TERMINAL_EIO;
                if (!(p.revents & (POLLHUP | POLLERR | POLLNVAL)))
                  break;
              }
            s->eof = 1;
          }
        else if (errno == EAGAIN || errno == EWOULDBLOCK)
          break;
        else
          return MULIB_IMTUI_TERMINAL_EIO;
      }
    s->escape_wait = !s->decoder.paste && s->decoder.length
                     && s->decoder.pending[0] == 27;
    return MULIB_IMTUI_TERMINAL_AGAIN;
  }

  int
  mulib_imtui_terminal_escape_pending (const mulib_imtui_terminal_t *s)
  {
    return s && s->state == mulib_imtui_terminal_active_ && s->escape_wait;
  }

  int
  mulib_imtui_terminal_flush_input (mulib_imtui_terminal_t *s,
                                   mulib_termkey_event_t *event)
  {
    if (!s || !event)
      return MULIB_IMTUI_TERMINAL_EINVAL;
    if (s->state != mulib_imtui_terminal_active_)
      return MULIB_IMTUI_TERMINAL_ESTATE;
    s->escape_wait = 0;
    return mulib_termkey_next (&s->decoder, event, 1) == 1
               ? MULIB_IMTUI_TERMINAL_EVENT : MULIB_IMTUI_TERMINAL_AGAIN;
  }

  static int
  mulib_imtui_terminal_release_ (mulib_imtui_terminal_t *s, int rc)
  {
    int error = rc ? errno : 0;
    if (mulib_termutils_raw_end (&s->raw))
      {
        if (!error)
          error = errno;
        /* raw_end preserves the allocation on failure for retry; this terminal
         * is being released even if the tty can no longer be restored. */
        free (s->raw.saved);
      }
    if (s->owned && close (s->input_fd) < 0 && !error)
      error = errno;
    mulib_strbuf_free (&s->enter);
    mulib_strbuf_free (&s->output);
    mulib_strbuf_free (&s->leave);
    memset (s, 0, sizeof (*s));
    if (error)
      {
        errno = error;
        return MULIB_IMTUI_TERMINAL_EIO;
      }
    return MULIB_IMTUI_TERMINAL_OK;
  }

  int
  mulib_imtui_terminal_close (mulib_imtui_terminal_t *s)
  {
    int rc;
    if (!s)
      return MULIB_IMTUI_TERMINAL_EINVAL;
    if (!s->state)
      return MULIB_IMTUI_TERMINAL_OK;
    s->state = mulib_imtui_terminal_closing_;
    rc = mulib_imtui_terminal_drain_ (s);
    if (!rc)
      rc = mulib_imtui_terminal_write_ (s->output_fd, &s->leave, &s->leave_offset);
    else if (rc == MULIB_IMTUI_TERMINAL_EIO && s->enter_offset)
      {
        int error = errno;
        (void)mulib_imtui_terminal_write_ (s->output_fd, &s->leave, &s->leave_offset);
        errno = error;
      }
    if (rc == MULIB_IMTUI_TERMINAL_AGAIN)
      return rc;
    return mulib_imtui_terminal_release_ (s, rc);
  }

  int
  mulib_imtui_terminal_abort (mulib_imtui_terminal_t *s)
  {
    int rc = MULIB_IMTUI_TERMINAL_OK;
    if (!s)
      return MULIB_IMTUI_TERMINAL_EINVAL;
    if (!s->state)
      return rc;
    if (s->enter_offset)
      rc = mulib_imtui_terminal_write_ (s->output_fd, &s->leave, &s->leave_offset);
    return mulib_imtui_terminal_release_ (s, rc);
  }

#ifdef __cplusplus
}
#endif
#endif /* MULIB_IMTUI_TERMINAL_IMPLEMENTED */
