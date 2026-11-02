/**
 * @file mulib-fswatch.h
 * @brief Native events, filesystem-neutral watch programs and daemon execution.
 * Define MULIB_IMPL_FSWATCH and the dependencies listed in the manual once.
 */
#ifndef MULIB_FSWATCH_H
#define MULIB_FSWATCH_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-daemon.h"
#include "mulib-glob.h"
#include "mulib-select.h"
#include "mulib-vfs.h"
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_FSWATCH_MODIFY = 0x00000002,
    MULIB_FSWATCH_ATTRIB = 0x00000004,
    MULIB_FSWATCH_CLOSE_WRITE = 0x00000008,
    MULIB_FSWATCH_MOVED_FROM = 0x00000040,
    MULIB_FSWATCH_MOVED_TO = 0x00000080,
    MULIB_FSWATCH_CREATE = 0x00000100,
    MULIB_FSWATCH_DELETE = 0x00000200,
    MULIB_FSWATCH_DELETE_SELF = 0x00000400,
    MULIB_FSWATCH_MOVE_SELF = 0x00000800,
    MULIB_FSWATCH_UNMOUNT = 0x00002000,
    MULIB_FSWATCH_OVERFLOW = 0x00004000,
    MULIB_FSWATCH_IGNORED = 0x00008000,
    MULIB_FSWATCH_ISDIR = 0x40000000,
    MULIB_FSWATCH_WATCH_MASK = 0x00000fce
  };
  typedef struct mulib_fswatch_event
  {
    int32_t watch;
    uint32_t mask, cookie;
    const char *name;
    size_t name_length;
  } mulib_fswatch_event;
  /** @brief Create an owned nonblocking, close-on-exec inotify descriptor. */
  int mulib_fswatch_create (void);
  /** @brief Add/replace a nonrecursive path watch using the documented mask
   * subset. */
  int mulib_fswatch_add (int fd, const char *path, unsigned mask);
  /** @brief Remove a watch; its queued IN_IGNORED notification remains
   * readable. */
  int mulib_fswatch_remove (int fd, int watch);
  /** @brief Read a native event batch; count unchanged on error. Minimum
   * capacity 16. */
  int mulib_fswatch_read (int fd, void *buffer, size_t capacity,
                          size_t *count);
  /** @brief Decode one event: 1 event, 0 end, -1 malformed (EINVAL); outputs
   * transactional. */
  int mulib_fswatch_next (const void *buffer, size_t length, size_t *offset,
                          mulib_fswatch_event *event);
  /** @brief Close once, releasing all watches but no filesystem objects. */
  int mulib_fswatch_close (int fd);
  typedef struct mulib_fswatch_program mulib_fswatch_program_t;
  typedef struct mulib_fswatch_runtime mulib_fswatch_runtime_t;
  typedef struct mulib_fswatch_error {
    size_t offset;
    const char *message; /**< Static diagnostic; NULL on success. */
  } mulib_fswatch_error_t;
  typedef struct mulib_fswatch_change {
    size_t watch;
    const char *root, *path; /**< Borrowed root and canonical relative path. */
    unsigned mask;
    int directory;
  } mulib_fswatch_change_t;
  /** Emit a snapshot entry. version must change when observable content changes.
   * Empty path represents the watched root itself. Return emit errors unchanged. */
  typedef int (*mulib_fswatch_snapshot_fn) (void *, const char *path,
                                            uint64_t version, int directory);
  /** Enumerate a complete snapshot; zero success, nonzero failure. Any filesystem
   * may implement this contract. Missing roots enumerate nothing successfully. */
  typedef int (*mulib_fswatch_scan_fn) (void *filesystem, const char *root,
      int recursive, mulib_fswatch_snapshot_fn emit, void *user);
  /** Named handler dispatch; all strings/events are borrowed for this call. */
  typedef int (*mulib_fswatch_handler_fn) (void *, const char *handler,
                                          const mulib_fswatch_change_t *);
  /** Parse <=64 KiB: watch "root" [recursive] { on create|modify|delete "glob"
   * -> handler; }. 'any' selects all three events; # comments are accepted.
   * Up to 64 watches and 256 rules; no filesystem access during parsing. */
  mulib_fswatch_program_t *mulib_fswatch_parse (const char *, size_t,
                                               mulib_fswatch_error_t *);
  /** Free program; it must outlive runtimes and calls borrowing it. */
  void mulib_fswatch_program_destroy (mulib_fswatch_program_t *);
  /** Number of roots; zero for NULL. */
  size_t mulib_fswatch_program_count (const mulib_fswatch_program_t *);
  /** Borrow a root and optionally query recursion. NULL for invalid index. */
  const char *mulib_fswatch_program_root (const mulib_fswatch_program_t *, size_t,
                                         int *recursive);
  /** Borrow program/backend, allocate a runtime. max_entries is 1..1000000,
   * including roots. No filesystem side effects until the first poll. */
  mulib_fswatch_runtime_t *mulib_fswatch_runtime_create (const mulib_fswatch_program_t *,
      mulib_fswatch_scan_fn, void *filesystem, size_t max_entries);
  /** Free snapshot storage; never frees the borrowed backend/program. */
  void mulib_fswatch_runtime_destroy (mulib_fswatch_runtime_t *);
  /** Scan and diff. First successful call establishes a silent baseline. Failed
   * scans preserve it. Commit each complete snapshot before delivering changes,
   * attempt every matching handler and return its first nonzero code. Internal
   * errors return -1/errno. Polling may miss changes between scans; no rename
   * pairing. Nested polls fail EBUSY. NULL handler is invalid. */
  int mulib_fswatch_poll (mulib_fswatch_runtime_t *, mulib_fswatch_handler_fn, void *);
  /** POSIX scanner: content fingerprints, recursive directories, symlinks not
   * followed. filesystem is unused. Non-POSIX returns -1/ENOSYS. */
  int mulib_fswatch_native_scan (void *, const char *, int,
                                mulib_fswatch_snapshot_fn, void *);
  /** MuLIB VFS scanner; filesystem is a borrowed mulib_vfs*. */
  int mulib_fswatch_vfs_scan (void *, const char *, int,
                             mulib_fswatch_snapshot_fn, void *);
  /** Launch a polling program through DAEMON. Child constructs its own runtime;
   * interval is 1..INT_MAX milliseconds. Handlers execute in the child. Setup
   * errors use DAEMON statuses; later scan/handler failures exit with status 1.
   * Stop with mulib_daemon_stop, then reap with mulib_process_wait. */
  int mulib_fswatch_launch (const mulib_daemon_t *, const mulib_fswatch_program_t *,
      mulib_fswatch_scan_fn, void *filesystem, size_t max_entries,
      mulib_fswatch_handler_fn, void *user, int interval_ms, mulib_process_t *child);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FSWATCH) && !defined(MULIB_FSWATCH_IMPLEMENTED)
#define MULIB_FSWATCH_IMPLEMENTED
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#if defined(__linux__)
#include <sys/inotify.h>
#include <unistd.h>
#endif
#ifdef __cplusplus
extern "C"
{
#endif
  int
  mulib_fswatch_create (void)
  {
#if defined(__linux__)
    return inotify_init1 (IN_NONBLOCK | IN_CLOEXEC);
#else
  errno = ENOSYS;
  return -1;
#endif
  }
  int
  mulib_fswatch_add (int fd, const char *path, unsigned mask)
  {
    if (fd < 0 || !path || !*path || !mask
        || (mask & ~(unsigned)MULIB_FSWATCH_WATCH_MASK))
      {
        errno = EINVAL;
        return -1;
      }
#if defined(__linux__)
    return inotify_add_watch (fd, path, (uint32_t)mask);
#else
  errno = ENOSYS;
  return -1;
#endif
  }
  int
  mulib_fswatch_remove (int fd, int watch)
  {
    if (fd < 0 || watch < 0)
      {
        errno = EINVAL;
        return -1;
      }
#if defined(__linux__)
    return inotify_rm_watch (fd, watch);
#else
  errno = ENOSYS;
  return -1;
#endif
  }
  int
  mulib_fswatch_read (int fd, void *buffer, size_t capacity, size_t *count)
  {
    if (fd < 0 || !buffer || !count || capacity < 16
        || capacity > (size_t)PTRDIFF_MAX)
      {
        errno = EINVAL;
        return -1;
      }
#if defined(__linux__)
    ssize_t n = read (fd, buffer, capacity);
    if (n < 0)
      return -1;
    *count = (size_t)n;
    return 0;
#else
  errno = ENOSYS;
  return -1;
#endif
  }
  int
  mulib_fswatch_next (const void *buffer, size_t length, size_t *offset,
                      mulib_fswatch_event *event)
  {
    const unsigned char *bytes;
    const char *zero;
    uint32_t fields[4];
    mulib_fswatch_event decoded;
    size_t remaining;
    if ((!buffer && length) || length > (size_t)PTRDIFF_MAX || !offset
        || !event || *offset > length)
      {
        errno = EINVAL;
        return -1;
      }
    if (*offset == length)
      return 0;
    remaining = length - *offset;
    if (remaining < 16)
      {
        errno = EINVAL;
        return -1;
      }
    bytes = (const unsigned char *)buffer + *offset;
    memcpy (fields, bytes, 16);
    if ((size_t)fields[3] > remaining - 16)
      {
        errno = EINVAL;
        return -1;
      }
    memcpy (&decoded.watch, bytes, 4);
    if (decoded.watch < -1)
      {
        errno = EINVAL;
        return -1;
      }
    decoded.mask = fields[1];
    decoded.cookie = fields[2];
    decoded.name = NULL;
    decoded.name_length = 0;
    if (fields[3])
      {
        decoded.name = (const char *)(bytes + 16);
        zero = (const char *)memchr (decoded.name, 0, fields[3]);
        if (!zero)
          {
            errno = EINVAL;
            return -1;
          }
        decoded.name_length = (size_t)(zero - decoded.name);
      }
    *event = decoded;
    *offset += 16 + (size_t)fields[3];
    return 1;
  }
  int
  mulib_fswatch_close (int fd)
  {
    if (fd < 0)
      {
        errno = EINVAL;
        return -1;
      }
#if defined(__linux__)
    return close (fd);
#else
  errno = ENOSYS;
  return -1;
#endif
  }
  typedef struct mulib_fswatch_root_ { char *path; int recursive; } mulib_fswatch_root_;
  typedef struct mulib_fswatch_rule_ {
    size_t watch;
    unsigned mask;
    char *pattern, *handler;
  } mulib_fswatch_rule_;
  struct mulib_fswatch_program {
    mulib_fswatch_root_ roots[64];
    mulib_fswatch_rule_ rules[256];
    size_t count, rule_count;
  };
  typedef struct mulib_fswatch_parser_ {
    const char *src;
    size_t length, at, token_at;
    int token;
    char text[4096];
    mulib_fswatch_error_t error;
  } mulib_fswatch_parser_;
  static int
  mulib_fswatch_fail_ (mulib_fswatch_parser_ *p, const char *message)
  {
    if (!p->error.message) { p->error.offset = p->token_at; p->error.message = message; }
    return 0;
  }
  static char *
  mulib_fswatch_copy_ (const char *s)
  {
    size_t n = strlen (s) + 1;
    char *p = (char *)malloc (n);
    if (p) memcpy (p, s, n);
    return p;
  }
  static int
  mulib_fswatch_alpha_ (unsigned char c)
  { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
  static int
  mulib_fswatch_token_ (mulib_fswatch_parser_ *p)
  {
    size_t n = 0;
    unsigned char c;
    for (;;) {
      while (p->at < p->length && strchr (" \t\r\n", p->src[p->at]) && p->src[p->at]) ++p->at;
      if (p->at < p->length && p->src[p->at] == '#') {
        while (p->at < p->length && p->src[p->at] != '\n') ++p->at;
      } else break;
    }
    p->token_at = p->at; p->text[0] = 0;
    if (p->at == p->length) { p->token = 0; return 1; }
    c = (unsigned char)p->src[p->at++];
    if (c == '"') {
      p->token = 257;
      while (p->at < p->length) {
        c = (unsigned char)p->src[p->at++];
        if (c == '"') { p->text[n] = 0; return 1; }
        if (c == '\\') {
          if (p->at == p->length) return mulib_fswatch_fail_ (p, "unterminated escape");
          c = (unsigned char)p->src[p->at++];
          if (c != '"' && c != '\\') return mulib_fswatch_fail_ (p, "only quote/backslash escapes are supported");
        }
        if (c < 32 || c == 127 || n == sizeof (p->text)-1) return mulib_fswatch_fail_ (p, "invalid or oversized string");
        p->text[n++] = (char)c;
      }
      return mulib_fswatch_fail_ (p, "unterminated string");
    }
    if (mulib_fswatch_alpha_ (c)) {
      p->token = 256; p->text[n++] = (char)c;
      while (p->at < p->length) {
        c = (unsigned char)p->src[p->at];
        if (!mulib_fswatch_alpha_ (c) && !(c >= '0' && c <= '9') && c != '-') break;
        if (n == 63) return mulib_fswatch_fail_ (p, "identifier exceeds 63 bytes");
        p->text[n++] = (char)c; ++p->at;
      }
      p->text[n] = 0; return 1;
    }
    if (c == '-' && p->at < p->length && p->src[p->at] == '>') {
      ++p->at; p->token = 258; return 1;
    }
    if (c && strchr ("{};|", c)) { p->token = c; return 1; }
    return mulib_fswatch_fail_ (p, "unexpected byte");
  }
  static int
  mulib_fswatch_expect_ (mulib_fswatch_parser_ *p, int token, const char *word)
  {
    if (p->token != token || (word && strcmp (word, p->text))) return mulib_fswatch_fail_ (p, "unexpected token");
    return mulib_fswatch_token_ (p);
  }
  mulib_fswatch_program_t *
  mulib_fswatch_parse (const char *src, size_t n, mulib_fswatch_error_t *error)
  {
    mulib_fswatch_parser_ p;
    mulib_fswatch_program_t *program;
    memset (&p, 0, sizeof (p)); p.src = src; p.length = n;
    program = (mulib_fswatch_program_t *)calloc (1, sizeof (*program));
    if ((!src && n) || n > 65536 || (n && memchr (src, 0, n))) mulib_fswatch_fail_ (&p, "invalid source or 64 KiB limit exceeded");
    else if (!program) mulib_fswatch_fail_ (&p, "allocation failed");
    else (void)mulib_fswatch_token_ (&p);
    while (!p.error.message && p.token) {
      mulib_fswatch_root_ *root;
      size_t watch;
      if (program->count == 64) { mulib_fswatch_fail_ (&p, "watch limit"); break; }
      if (!mulib_fswatch_expect_ (&p, 256, "watch")) break;
      if (p.token != 257 || !p.text[0]) { mulib_fswatch_fail_ (&p, "expected nonempty root string"); break; }
      watch = program->count++; root = program->roots + watch;
      root->path = mulib_fswatch_copy_ (p.text);
      if (!root->path) { mulib_fswatch_fail_ (&p, "allocation failed"); break; }
      if (!mulib_fswatch_token_ (&p)) break;
      if (p.token == 256 && !strcmp (p.text, "recursive")) {
        root->recursive = 1; if (!mulib_fswatch_token_ (&p)) break;
      }
      if (!mulib_fswatch_expect_ (&p, '{', NULL)) break;
      while (!p.error.message && p.token != '}') {
        mulib_fswatch_rule_ *r;
        if (program->rule_count == 256) { mulib_fswatch_fail_ (&p, "rule limit"); break; }
        if (!mulib_fswatch_expect_ (&p, 256, "on")) break;
        r = program->rules + program->rule_count++; r->watch = watch;
        do {
          if (p.token != 256) { mulib_fswatch_fail_ (&p, "expected event name"); break; }
          if (!strcmp (p.text, "create")) r->mask |= MULIB_FSWATCH_CREATE;
          else if (!strcmp (p.text, "modify")) r->mask |= MULIB_FSWATCH_MODIFY;
          else if (!strcmp (p.text, "delete")) r->mask |= MULIB_FSWATCH_DELETE;
          else if (!strcmp (p.text, "any")) r->mask |= MULIB_FSWATCH_CREATE | MULIB_FSWATCH_MODIFY | MULIB_FSWATCH_DELETE;
          else { mulib_fswatch_fail_ (&p, "unknown event name"); break; }
          if (!mulib_fswatch_token_ (&p) || p.token != '|') break;
          if (!mulib_fswatch_token_ (&p)) break;
        } while (!p.error.message);
        if (p.error.message) break;
        if (p.token != 257) { mulib_fswatch_fail_ (&p, "expected glob string"); break; }
        r->pattern = mulib_fswatch_copy_ (p.text);
        if (!r->pattern) { mulib_fswatch_fail_ (&p, "allocation failed"); break; }
        if (!mulib_fswatch_token_ (&p) || !mulib_fswatch_expect_ (&p, 258, NULL)) break;
        if (p.token != 256) { mulib_fswatch_fail_ (&p, "expected handler identifier"); break; }
        r->handler = mulib_fswatch_copy_ (p.text);
        if (!r->handler) { mulib_fswatch_fail_ (&p, "allocation failed"); break; }
        if (!mulib_fswatch_token_ (&p) || !mulib_fswatch_expect_ (&p, ';', NULL)) break;
      }
      if (p.error.message || !mulib_fswatch_expect_ (&p, '}', NULL)) break;
    }
    if (error) *error = p.error;
    if (p.error.message) { mulib_fswatch_program_destroy (program); return NULL; }
    return program;
  }
  void
  mulib_fswatch_program_destroy (mulib_fswatch_program_t *p)
  {
    size_t i;
    if (!p) return;
    for (i = 0; i < p->count; ++i) free (p->roots[i].path);
    for (i = 0; i < p->rule_count; ++i) { free (p->rules[i].pattern); free (p->rules[i].handler); }
    free (p);
  }
  size_t mulib_fswatch_program_count (const mulib_fswatch_program_t *p) { return p ? p->count : 0; }
  const char *
  mulib_fswatch_program_root (const mulib_fswatch_program_t *p, size_t i, int *recursive)
  {
    if (!p || i >= p->count) return NULL;
    if (recursive) *recursive = p->roots[i].recursive;
    return p->roots[i].path;
  }
  typedef struct mulib_fswatch_entry_ {
    char *path;
    size_t watch;
    uint64_t version;
    int directory;
  } mulib_fswatch_entry_;
  typedef struct mulib_fswatch_snapshot_ {
    mulib_fswatch_entry_ *entries;
    size_t count, capacity, limit, watch;
    int failed;
  } mulib_fswatch_snapshot_;
  struct mulib_fswatch_runtime {
    const mulib_fswatch_program_t *program;
    mulib_fswatch_scan_fn scan;
    void *filesystem;
    mulib_fswatch_snapshot_ snapshot;
    int initialized, polling;
  };
  static void
  mulib_fswatch_snapshot_free_ (mulib_fswatch_snapshot_ *s)
  {
    size_t i;
    for (i = 0; i < s->count; ++i) free (s->entries[i].path);
    free (s->entries);
  }
  static int
  mulib_fswatch_relative_ (const char *s)
  {
    size_t i, start = 0;
    if (!s) return 0;
    if (!*s) return 1;
    for (i = 0;; ++i) {
      unsigned char c = (unsigned char)s[i];
      if (i > 4095 || (c && (c < 32 || c == 127 || c == '\\'))) return 0;
      if (!c || c == '/') {
        size_t n = i-start;
        if (!n || (n == 1 && s[start] == '.') || (n == 2 && s[start] == '.' && s[start+1] == '.')) return 0;
        if (!c) return 1;
        start = i+1;
      }
    }
  }
  static int
  mulib_fswatch_collect_ (void *user, const char *path, uint64_t version, int directory)
  {
    mulib_fswatch_snapshot_ *s = (mulib_fswatch_snapshot_ *)user;
    mulib_fswatch_entry_ *e;
    if (s->failed) { errno = s->failed; return -1; }
    if (!mulib_fswatch_relative_ (path)) { s->failed = EINVAL; errno = EINVAL; return -1; }
    if (s->count == s->limit) { s->failed = EOVERFLOW; errno = EOVERFLOW; return -1; }
    if (s->count == s->capacity) {
      size_t cap = s->capacity ? s->capacity * 2 : 32;
      if (cap > s->limit) cap = s->limit;
      e = (mulib_fswatch_entry_ *)realloc (s->entries, cap * sizeof (*e));
      if (!e) { s->failed = ENOMEM; errno = ENOMEM; return -1; }
      s->entries = e; s->capacity = cap;
    }
    e = s->entries + s->count;
    e->path = mulib_fswatch_copy_ (path);
    if (!e->path) { s->failed = ENOMEM; errno = ENOMEM; return -1; }
    e->watch = s->watch; e->version = version; e->directory = !!directory; ++s->count;
    return 0;
  }
  static int
  mulib_fswatch_compare_ (const void *a, const void *b)
  {
    const mulib_fswatch_entry_ *x = (const mulib_fswatch_entry_ *)a, *y = (const mulib_fswatch_entry_ *)b;
    if (x->watch != y->watch) return x->watch < y->watch ? -1 : 1;
    return strcmp (x->path, y->path);
  }
  mulib_fswatch_runtime_t *
  mulib_fswatch_runtime_create (const mulib_fswatch_program_t *p, mulib_fswatch_scan_fn scan,
                               void *filesystem, size_t limit)
  {
    mulib_fswatch_runtime_t *r;
    if (!p || !scan || !limit || limit > 1000000 || limit > SIZE_MAX / sizeof (mulib_fswatch_entry_)) { errno = EINVAL; return NULL; }
    r = (mulib_fswatch_runtime_t *)calloc (1, sizeof (*r));
    if (!r) return NULL;
    r->program = p; r->scan = scan; r->filesystem = filesystem; r->snapshot.limit = limit;
    return r;
  }
  void
  mulib_fswatch_runtime_destroy (mulib_fswatch_runtime_t *r)
  {
    if (!r || r->polling) return;
    mulib_fswatch_snapshot_free_ (&r->snapshot); free (r);
  }
  static int
  mulib_fswatch_dispatch_ (const mulib_fswatch_program_t *p, const mulib_fswatch_entry_ *entry,
                           unsigned mask, mulib_fswatch_handler_fn fn, void *user)
  {
    size_t i;
    int first = 0;
    mulib_fswatch_change_t event;
    event.watch = entry->watch; event.root = p->roots[entry->watch].path;
    event.path = entry->path; event.directory = entry->directory; event.mask = mask;
    for (i = 0; i < p->rule_count; ++i) {
      const mulib_fswatch_rule_ *rule = p->rules + i;
      if (rule->watch == event.watch && (rule->mask & mask) && mulib_glob_match (rule->pattern, event.path)) {
        int rc = fn (user, rule->handler, &event);
        if (!first) first = rc;
      }
    }
    return first;
  }
  int
  mulib_fswatch_poll (mulib_fswatch_runtime_t *r, mulib_fswatch_handler_fn fn, void *user)
  {
    mulib_fswatch_snapshot_ next, old;
    size_t i, a = 0, b = 0;
    int first = 0;
    if (!r || !fn) { errno = EINVAL; return -1; }
    if (r->polling) { errno = EBUSY; return -1; }
    r->polling = 1;
    memset (&next, 0, sizeof (next)); next.limit = r->snapshot.limit;
    for (i = 0; i < r->program->count; ++i) {
      int rc;
      next.watch = i;
      rc = r->scan (r->filesystem, r->program->roots[i].path, r->program->roots[i].recursive, mulib_fswatch_collect_, &next);
      if (rc || next.failed) {
        int saved = next.failed ? next.failed : errno;
        mulib_fswatch_snapshot_free_ (&next); r->polling = 0; errno = saved; return rc ? rc : -1;
      }
    }
    if (next.count) qsort (next.entries, next.count, sizeof (*next.entries), mulib_fswatch_compare_);
    for (i = 1; i < next.count; ++i)
      if (!mulib_fswatch_compare_ (next.entries+i-1, next.entries+i)) {
        mulib_fswatch_snapshot_free_ (&next); r->polling = 0; errno = EINVAL; return -1;
      }
    old = r->snapshot; r->snapshot = next;
    if (r->initialized) while (a < old.count || b < next.count) {
      int cmp = a == old.count ? 1 : b == next.count ? -1 : mulib_fswatch_compare_ (old.entries+a, next.entries+b);
      int rc = 0;
      if (cmp < 0) rc = mulib_fswatch_dispatch_ (r->program, old.entries+a++, MULIB_FSWATCH_DELETE, fn, user);
      else if (cmp > 0) rc = mulib_fswatch_dispatch_ (r->program, next.entries+b++, MULIB_FSWATCH_CREATE, fn, user);
      else {
        if (old.entries[a].version != next.entries[b].version || old.entries[a].directory != next.entries[b].directory)
          rc = mulib_fswatch_dispatch_ (r->program, next.entries+b, MULIB_FSWATCH_MODIFY, fn, user);
        ++a; ++b;
      }
      if (!first) first = rc;
    }
    r->initialized = 1; r->polling = 0; mulib_fswatch_snapshot_free_ (&old);
    return first;
  }
  static uint64_t
  mulib_fswatch_hash_ (uint64_t h, const void *bytes, size_t n)
  {
    const unsigned char *p = (const unsigned char *)bytes;
    size_t i;
    for (i = 0; i < n; ++i) { h ^= p[i]; h *= UINT64_C(1099511628211); }
    return h;
  }
  static int
  mulib_fswatch_join_ (char out[4096], const char *base, const char *child)
  {
    size_t n = strlen (base), m = strlen (child);
    int slash = n && base[n-1] != '/';
    if (n >= 4096 || m >= 4096 || n + m + (size_t)slash >= 4096) { errno = ENAMETOOLONG; return -1; }
    memcpy (out, base, n);
    if (slash) out[n++] = '/';
    memcpy (out+n, child, m+1); return 0;
  }
  typedef struct mulib_fswatch_vfs_context_ {
    mulib_vfs *vfs;
    const char *path, *relative;
    int recursive;
    unsigned depth;
    mulib_fswatch_snapshot_fn emit;
    void *user;
    int *failure, *saved_errno;
  } mulib_fswatch_vfs_context_;
  static int mulib_fswatch_vfs_walk_ (mulib_fswatch_vfs_context_ *);
  static int
  mulib_fswatch_vfs_child_ (void *user, const char *name, const mulib_vfs_stat *info)
  {
    mulib_fswatch_vfs_context_ *parent = (mulib_fswatch_vfs_context_ *)user, child = *parent;
    char path[4096], relative[4096];
    int rc;
    (void)info;
    if (*parent->failure) { errno = *parent->saved_errno; return *parent->failure; }
    if (!name || !*name || strchr (name, '/') || !mulib_fswatch_relative_ (name)) { errno = EINVAL; rc = -1; }
    else if (mulib_fswatch_join_ (path, parent->path, name) || mulib_fswatch_join_ (relative, parent->relative, name)) rc = -1;
    else {
      child.path = path; child.relative = relative; ++child.depth;
      rc = mulib_fswatch_vfs_walk_ (&child);
    }
    if (rc) { *parent->failure = rc; *parent->saved_errno = errno; }
    return rc;
  }
  static int
  mulib_fswatch_vfs_walk_ (mulib_fswatch_vfs_context_ *c)
  {
    mulib_vfs_stat info;
    uint64_t hash = UINT64_C(14695981039346656037);
    unsigned char buffer[4096];
    size_t offset = 0, n;
    int rc;
    if (c->depth > 128) { errno = EOVERFLOW; return -1; }
    rc = mulib_vfs_getstat (c->vfs, c->path, &info);
    if (rc == MULIB_VFS_ENOENT && !c->depth) return 0;
    if (rc) { errno = EIO; return -1; }
    if (info.kind == MULIB_VFS_FILE) do {
      rc = mulib_vfs_read (c->vfs, c->path, offset, buffer, sizeof (buffer), &n);
      if (rc || n > SIZE_MAX-offset) { errno = EIO; return -1; }
      hash = mulib_fswatch_hash_ (hash, buffer, n); offset += n;
    } while (n == sizeof (buffer));
    rc = c->emit (c->user, c->relative, hash, info.kind == MULIB_VFS_DIRECTORY);
    if (rc || info.kind != MULIB_VFS_DIRECTORY || (c->depth && !c->recursive)) return rc;
    rc = mulib_vfs_list (c->vfs, c->path, mulib_fswatch_vfs_child_, c);
    if (*c->failure) { errno = *c->saved_errno; return *c->failure; }
    if (rc) { errno = EIO; return -1; }
    return 0;
  }
  int
  mulib_fswatch_vfs_scan (void *filesystem, const char *root, int recursive,
                         mulib_fswatch_snapshot_fn emit, void *user)
  {
    mulib_fswatch_vfs_context_ c;
    int failure = 0, saved_errno = 0;
    if (!filesystem || !root || !emit) { errno = EINVAL; return -1; }
    c.vfs = (mulib_vfs *)filesystem; c.path = root; c.relative = "";
    c.recursive = recursive; c.depth = 0; c.emit = emit; c.user = user;
    c.failure = &failure; c.saved_errno = &saved_errno;
    return mulib_fswatch_vfs_walk_ (&c);
  }
#if defined(__unix__) || defined(__APPLE__)
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
  extern int lstat (const char *, struct stat *);
  extern ssize_t readlink (const char *, char *, size_t);
  static int
  mulib_fswatch_native_walk_ (const char *path, const char *relative, int recursive,
      unsigned depth, mulib_fswatch_snapshot_fn emit, void *user)
  {
    struct stat info;
    uint64_t hash = UINT64_C(14695981039346656037), mode;
    char buffer[4096];
    int rc;
    if (depth > 128) { errno = EOVERFLOW; return -1; }
    if (lstat (path, &info)) return errno == ENOENT && !depth ? 0 : -1;
    mode = (uint64_t)info.st_mode; hash = mulib_fswatch_hash_ (hash, &mode, sizeof (mode));
    if (S_ISREG (info.st_mode)) {
      FILE *f = fopen (path, "rb");
      size_t n;
      if (!f) return -1;
      do { n = fread (buffer, 1, sizeof (buffer), f); hash = mulib_fswatch_hash_ (hash, buffer, n); } while (n == sizeof (buffer));
      if (ferror (f)) { fclose (f); errno = EIO; return -1; }
      if (fclose (f)) return -1;
    } else if (S_ISLNK (info.st_mode)) {
      ssize_t n = readlink (path, buffer, sizeof (buffer));
      if (n < 0) return -1;
      if ((size_t)n == sizeof (buffer)) { errno = ENAMETOOLONG; return -1; }
      hash = mulib_fswatch_hash_ (hash, buffer, (size_t)n);
    }
    rc = emit (user, relative, hash, S_ISDIR (info.st_mode));
    if (rc || !S_ISDIR (info.st_mode) || (depth && !recursive)) return rc;
    {
      DIR *dir = opendir (path);
      struct dirent *entry;
      if (!dir) return -1;
      for (;;) {
        char child[4096], rel[4096];
        errno = 0; entry = readdir (dir);
        if (!entry) { rc = errno ? -1 : 0; break; }
        if (!strcmp (entry->d_name, ".") || !strcmp (entry->d_name, "..")) continue;
        if (mulib_fswatch_join_ (child, path, entry->d_name) || mulib_fswatch_join_ (rel, relative, entry->d_name)) { rc = -1; break; }
        rc = mulib_fswatch_native_walk_ (child, rel, recursive, depth+1, emit, user);
        if (rc) break;
      }
      { int saved = errno; if (closedir (dir) && !rc) return -1; errno = saved; }
    }
    return rc;
  }
  int
  mulib_fswatch_native_scan (void *filesystem, const char *root, int recursive,
                            mulib_fswatch_snapshot_fn emit, void *user)
  {
    (void)filesystem;
    if (!root || !*root || !emit) { errno = EINVAL; return -1; }
    return mulib_fswatch_native_walk_ (root, "", recursive, 0, emit, user);
  }
#else
  int mulib_fswatch_native_scan (void *fs, const char *root, int rec, mulib_fswatch_snapshot_fn fn, void *user)
  { (void)fs; (void)root; (void)rec; (void)fn; (void)user; errno = ENOSYS; return -1; }
#endif
  typedef struct mulib_fswatch_daemon_ {
    const mulib_fswatch_program_t *program;
    mulib_fswatch_scan_fn scan;
    void *filesystem, *user;
    size_t limit;
    mulib_fswatch_handler_fn handler;
    int interval;
  } mulib_fswatch_daemon_;
  static int
  mulib_fswatch_daemon_entry_ (void *user)
  {
    mulib_fswatch_daemon_ *d = (mulib_fswatch_daemon_ *)user;
    mulib_fswatch_runtime_t *r = mulib_fswatch_runtime_create (d->program, d->scan, d->filesystem, d->limit);
    if (!r) return 1;
    for (;;) {
      if (mulib_fswatch_poll (r, d->handler, d->user)) break;
      if (mulib_select_wait (NULL, 0, d->interval) < 0 && errno != EINTR) break;
    }
    mulib_fswatch_runtime_destroy (r); return 1;
  }
  int
  mulib_fswatch_launch (const mulib_daemon_t *spec, const mulib_fswatch_program_t *program,
      mulib_fswatch_scan_fn scan, void *filesystem, size_t limit, mulib_fswatch_handler_fn handler,
      void *user, int interval, mulib_process_t *child)
  {
    mulib_fswatch_daemon_ d;
    if (!program || !scan || !handler || !limit || limit > 1000000 || interval < 1) return MULIB_DAEMON_INVALID;
    d.program = program; d.scan = scan; d.filesystem = filesystem; d.limit = limit;
    d.handler = handler; d.user = user; d.interval = interval;
    return mulib_daemon_launch (spec, mulib_fswatch_daemon_entry_, &d, child);
  }
#ifdef __cplusplus
}
#endif
#endif
