/**
 * @file mulib-kronvm.h
 * @brief Persistent KronQL promises with caller-clocked dispatch.
 * Define MULIB_IMPL_KRONVM and the dependency switches listed in
 * the manual.
 */
#ifndef MULIB_KRONVM_H
#define MULIB_KRONVM_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_kronvm mulib_kronvm_t;
  typedef struct mulib_kronvm_result
  {
    uint64_t id;
    int64_t deadline;
    int launched, wait_status, execution_errno;
  } mulib_kronvm_result_t;
  /** Open or create a private cache directory (NULL selects /var/run/kronvm).
   * Exclusive ownership until close. Return zero or -1 with errno. */
  int mulib_kronvm_open (const char *directory, mulib_kronvm_t **out);
  /** Release memory and the cache lock. Pending promises survive close. */
  void mulib_kronvm_close (mulib_kronvm_t *vm);
  /** Parse one statement, persist its promise and return its stable ID.
   * Mutation returns 0 on success, 1 on visible commit with uncertain
   * durability, or -1 with errno before commit. All clocks are nonnegative
   * Unix seconds. */
  int mulib_kronvm_delegate (mulib_kronvm_t *vm, const char *source,
                             int64_t now, uint64_t *id);
  /** Cancel a promise by ID. Missing ID sets ENOENT. */
  int mulib_kronvm_pull (mulib_kronvm_t *vm, uint64_t id);
  /** Atomically cancel all promises awaiting this positive logical signal. */
  int mulib_kronvm_signal (mulib_kronvm_t *vm, int number);
  /** Read the earliest deadline. Returns 1 if present, 0 empty, -1
   * invalid. */
  int mulib_kronvm_next (const mulib_kronvm_t *vm, int64_t *deadline);
  /** Consume at most one due occurrence. Return 0 idle, 1 consumed, 2 consumed
   * with uncertain durability, -1 pre-commit error. Inspect result for
   * execution errors. Expired occurrences are consumed without launching. */
  int mulib_kronvm_dispatch (mulib_kronvm_t *vm, int64_t now,
                             mulib_kronvm_result_t *result);

  /* Application-owned job profile. It uses the same caller clock, queue and
   * snapshot transactions, but is incompatible with the KronQL shell cache. */
#define MULIB_KRONVM_MAX_JOB_BYTES 65536u
  typedef enum mulib_kronvm_cadence
  {
    MULIB_KRONVM_ONCE, MULIB_KRONVM_EVERY
  } mulib_kronvm_cadence_t;
  typedef enum mulib_kronvm_lifetime
  {
    MULIB_KRONVM_PERSISTENT, MULIB_KRONVM_FOR, MULIB_KRONVM_UNTIL_SIGNAL
  } mulib_kronvm_lifetime_t;
  typedef struct mulib_kronvm_schedule
  {
    mulib_kronvm_cadence_t cadence;
    int64_t seconds; /* absolute nonnegative due time or positive interval */
    mulib_kronvm_lifetime_t lifetime;
    int64_t limit;   /* zero, positive duration, or positive logical signal */
  } mulib_kronvm_schedule_t;
  typedef void (*mulib_kronvm_job_fn) (void *, uint64_t, int64_t,
                                     const unsigned char *, size_t);
  /** Open an explicit, existing owner-only directory. An existing snapshot
   * must be an owner-only regular file. The host must keep the path and its
   * parents stable and trusted. Return -1 on failure or 0/1 with a live VM
   * (1 if initial snapshot is visible but durability unconfirmed). Close,
   * pull, signal and next have the same contracts in both profiles. */
  int mulib_kronvm_open_jobs (const char *directory, mulib_kronvm_t **out);
  /** Check a data-job schedule without allocating or accessing the VM. */
  int mulib_kronvm_validate_job_schedule (
      const mulib_kronvm_schedule_t *schedule, int64_t now);
  /** Schedule a copied, nonempty opaque data descriptor, not KronQL. */
  int mulib_kronvm_delegate_job (mulib_kronvm_t *vm,
                                 const mulib_kronvm_schedule_t *schedule,
                                 int64_t now, const void *data, size_t size,
                                 uint64_t *id);
  /** Same consumption status as dispatch. The hook runs only after a durable
   * commit and before return, for an unexpired job. Borrowed bytes are only
   * valid during the hook; it must not reenter the VM. No child is launched.
   * The application hook is responsible for validation and authorization. */
  int mulib_kronvm_dispatch_job (mulib_kronvm_t *vm, int64_t now,
                                 mulib_kronvm_job_fn hook, void *user,
                                 uint64_t *id, int64_t *deadline);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_KRONVM) && !defined(MULIB_KRONVM_IMPLEMENTED)
#define MULIB_KRONVM_IMPLEMENTED
#include "mulib-date.h"
#include "mulib-endian.h"
#include "mulib-evtsched.h"
#include "mulib-kvdb.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct mulib_kronvm_task
{
  mulib_evtsched_notice_t notice;
  struct mulib_kronvm_task *next;
  uint64_t id, ticket;
  int64_t interval, end, start, first;
  int signal, shell;
  char *source, *command;
  unsigned char *job;
  size_t job_size;
} mulib_kronvm_task;
struct mulib_kronvm
{
  mulib_evtsched_t queue;
  mulib_kronvm_task *list;
  char *path;
  int lock, jobs;
  uint64_t serial;
};
static int
mulib_kronvm_fail (int error)
{
  errno = error;
  return -1;
}
static void
mulib_kronvm_free (mulib_kronvm_task *p)
{
  if (p)
    {
      free (p->source);
      free (p->command);
      free (p->job);
      free (p);
    }
}
static void
mulib_kronvm_space (const char **s)
{
  *s += strspn (*s, " \t\n\r");
}
static int
mulib_kronvm_token (const char **s, const char *word)
{
  size_t n = strlen (word);
  mulib_kronvm_space (s);
  if (strncmp (*s, word, n))
    return 0;
  if ((word[0] >= 'A' && word[0] <= 'Z') && (*s)[n]
      && strchr (
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_",
          (*s)[n]))
    return 0;
  *s += n;
  return 1;
}
static int
mulib_kronvm_uint (const char **s, int64_t *v)
{
  int64_t n = 0;
  mulib_kronvm_space (s);
  if (**s < '0' || **s > '9')
    return 0;
  while (**s >= '0' && **s <= '9')
    {
      int digit = *(*s)++ - '0';
      if (n > (INT64_MAX - digit) / 10)
        return 0;
      n = n * 10 + digit;
    }
  *v = n;
  return 1;
}
static int
mulib_kronvm_duration (const char **s, int64_t *v)
{
  int64_t n, scale;
  if (!mulib_kronvm_token (s, "<") || !mulib_kronvm_uint (s, &n) || !n)
    return 0;
  switch (**s)
    {
    case 's':
      scale = 1;
      break;
    case 'm':
      scale = 60;
      break;
    case 'h':
      scale = 3600;
      break;
    case 'd':
      scale = 86400;
      break;
    default:
      return 0;
    }
  ++*s;
  if (n > INT64_MAX / scale || !mulib_kronvm_token (s, ">"))
    return 0;
  *v = n * scale;
  return 1;
}
static int64_t
mulib_kronvm_year_start (int y)
{
  int prior = y - 1;
  return (int64_t)((y - 1970) * 365 + prior / 4 - prior / 100 + prior / 400
                   - 477)
         * 86400;
}
/* Resolve a yearless UTC date to the first occurrence on or after admission.
 */
static int
mulib_kronvm_absolute (const char **s, int64_t now, int64_t *due)
{
  static const char *const months[]
      = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
  int64_t h, m, sec, d;
  if (!mulib_kronvm_token (s, "<") || !mulib_kronvm_uint (s, &h))
    return 0;
  if (mulib_kronvm_token (s, ">"))
    {
      *due = h;
      return 1;
    }
  if (h > 23 || !mulib_kronvm_token (s, ":") || !mulib_kronvm_uint (s, &m)
      || m > 59 || !mulib_kronvm_token (s, ":") || !mulib_kronvm_uint (s, &sec)
      || sec > 59 || !mulib_kronvm_token (s, ";"))
    return 0;
  unsigned month = 1;
  while (month <= 12 && !mulib_kronvm_token (s, months[month - 1]))
    ++month;
  mulib_kronvm_space (s);
  if (month > 12 || !mulib_kronvm_uint (s, &d) || d < 1
      || d > mulib_date_days_in_month (2000, month)
      || !mulib_kronvm_token (s, ">"))
    return 0;
  int lo = 1970, hi = 10000;
  while (hi - lo > 1)
    {
      int mid = lo + (hi - lo) / 2;
      if (mulib_kronvm_year_start (mid) <= now)
        lo = mid;
      else
        hi = mid;
    }
  for (int y = lo; y < 10000; ++y)
    {
      if (!mulib_date_valid (y, month, (unsigned)d))
        continue;
      int64_t days = d - 1;
      for (unsigned i = 1; i < month; ++i)
        days += mulib_date_days_in_month (y, i);
      *due = mulib_kronvm_year_start (y) + days * 86400 + h * 3600 + m * 60
             + sec;
      if (*due >= now)
        return 1;
    }
  return 0;
}
static mulib_kronvm_task *
mulib_kronvm_parse (const char *source, int64_t now)
{
  const char *s = source, *begin;
  mulib_kronvm_task *p = (mulib_kronvm_task *)calloc (1, sizeof *p);
  if (!p)
    return NULL;
  p->start = now;
  p->end = INT64_MAX;
  p->shell = mulib_kronvm_token (&s, "DELEGATE");
  if (!p->shell && !mulib_kronvm_token (&s, "EXECUTE"))
    goto syntax;
  mulib_kronvm_space (&s);
  {
    char quote = p->shell ? '\x60' : '"';
    if (*s++ != quote)
      goto syntax;
    begin = s;
    s = strchr (s, quote);
    if (!s || s == begin || (!p->shell && *begin != '/'))
      goto syntax;
    size_t n = (size_t)(s - begin);
    p->command = (char *)malloc (n + 1);
    if (!p->command)
      goto error;
    memcpy (p->command, begin, n);
    p->command[n] = 0;
    ++s;
  }
  if (mulib_kronvm_token (&s, "ONCE"))
    {
      if (!mulib_kronvm_token (&s, "AT")
          || !mulib_kronvm_absolute (&s, now, &p->notice.deadline))
        goto syntax;
    }
  else
    {
      if (!mulib_kronvm_token (&s, "EVERY")
          || !mulib_kronvm_duration (&s, &p->interval)
          || p->interval > INT64_MAX - now)
        goto syntax;
      p->notice.deadline = now + p->interval;
    }
  if (mulib_kronvm_token (&s, "FOR"))
    {
      int64_t duration;
      if (!mulib_kronvm_duration (&s, &duration) || duration > INT64_MAX - now)
        goto syntax;
      p->end = now + duration;
      if (p->notice.deadline > p->end)
        goto syntax;
    }
  else if (mulib_kronvm_token (&s, "UNTIL"))
    {
      int64_t n;
      if (!mulib_kronvm_token (&s, "SIGNAL") || !mulib_kronvm_token (&s, "("))
        goto syntax;
      mulib_kronvm_space (&s);
      if (!mulib_kronvm_uint (&s, &n) || !n || n > INT_MAX
          || !mulib_kronvm_token (&s, ")"))
        goto syntax;
      p->signal = (int)n;
    }
  else
    (void)mulib_kronvm_token (&s, "PERSISTENT");
  if (!mulib_kronvm_token (&s, ";"))
    goto syntax;
  mulib_kronvm_space (&s);
  if (*s)
    goto syntax;
  p->source = (char *)malloc (strlen (source) + 1);
  if (!p->source)
    goto error;
  strcpy (p->source, source);
  p->notice.data = p;
  return p;
syntax:
  errno = EINVAL;
error:
  mulib_kronvm_free (p);
  return NULL;
}
static int
mulib_kronvm_db_error (int rc)
{
  switch (rc)
    {
    case MULIB_KVDB_ENOMEM:
      return mulib_kronvm_fail (ENOMEM);
    case MULIB_KVDB_EOVERFLOW:
      return mulib_kronvm_fail (EOVERFLOW);
    case MULIB_KVDB_ECORRUPT:
      return mulib_kronvm_fail (EINVAL);
    default:
      return mulib_kronvm_fail (EIO);
    }
}
static const char *
mulib_kronvm_profile_key (const mulib_kronvm_t *vm)
{
  return vm->jobs ? "KronJobV1" : "KronVM1";
}
static int
mulib_kronvm_highwater (const mulib_kronvm_t *vm, mulib_kvdb_t *db,
                        uint64_t serial)
{
  const char *key = mulib_kronvm_profile_key (vm);
  unsigned char bytes[8];
  mulib_endian_store_be64 (bytes, serial);
  return mulib_kvdb_set (db, key, strlen (key), bytes, sizeof bytes);
}
static void
mulib_kronvm_attach (mulib_kronvm_t *vm, mulib_kronvm_task *p)
{
  p->next = vm->list;
  vm->list = p;
  mulib_evtsched_schedule (&vm->queue, &p->notice, p->notice.deadline);
}
static int
mulib_kronvm_read_highwater (const mulib_kronvm_t *vm, mulib_kvdb_t *db,
                             uint64_t *serial)
{
  const void *v;
  size_t vn;
  const char *key = mulib_kronvm_profile_key (vm);
  if (mulib_kvdb_get (db, key, strlen (key), &v, &vn) || vn != 8)
    return mulib_kronvm_fail (EINVAL);
  *serial = mulib_endian_load_be64 ((const unsigned char *)v);
  return 0;
}

/* Canonical v1 job record: id, start, due, first, interval, end and signal
 * (all big-endian u64), then the raw descriptor bytes. No native structs,
 * text parser, paths, callback code or context get written to disk. */
enum { MULIB_KRONVM_JOB_PREFIX = 7 * 8 };
static mulib_kronvm_task *
mulib_kronvm_decode_job (const unsigned char *b, size_t vn, uint64_t ticket,
                        uint64_t id, uint64_t start, uint64_t due)
{
  if (vn <= MULIB_KRONVM_JOB_PREFIX
      || vn - MULIB_KRONVM_JOB_PREFIX > MULIB_KRONVM_MAX_JOB_BYTES)
    {
      errno = EINVAL;
      return NULL;
    }
  uint64_t first = mulib_endian_load_be64 (b + 24);
  uint64_t interval = mulib_endian_load_be64 (b + 32);
  uint64_t end = mulib_endian_load_be64 (b + 40);
  uint64_t signal = mulib_endian_load_be64 (b + 48);
  if (first > due || due > end || end > INT64_MAX || start > end
      || (end != INT64_MAX && end == start)
      || signal > INT32_MAX || signal > INT_MAX
      || (signal && end != INT64_MAX)
      || (interval ? (first < start || first - start != interval
                      || (due - first) % interval)
                   : (due != first))
      || ((id == ticket) != (due == first)))
    {
      errno = EINVAL;
      return NULL;
    }
  mulib_kronvm_task *p = (mulib_kronvm_task *)calloc (1, sizeof *p);
  if (!p)
    return NULL;
  p->job_size = vn - MULIB_KRONVM_JOB_PREFIX;
  p->job = (unsigned char *)malloc (p->job_size);
  if (!p->job)
    {
      mulib_kronvm_free (p);
      errno = ENOMEM;
      return NULL;
    }
  memcpy (p->job, b + MULIB_KRONVM_JOB_PREFIX, p->job_size);
  p->notice.data = p;
  p->start = (int64_t)start;
  p->first = (int64_t)first;
  p->interval = (int64_t)interval;
  p->end = (int64_t)end;
  p->signal = (int)signal;
  return p;
}

static int
mulib_kronvm_restore (mulib_kronvm_t *vm, mulib_kvdb_t *db)
{
  const void *v, *key;
  size_t vn, kn;
  if (mulib_kronvm_read_highwater (vm, db, &vm->serial) < 0)
    return -1;
  const char *profile = mulib_kronvm_profile_key (vm);
  const size_t profile_len = strlen (profile);
  for (size_t i = 0; i < mulib_kvdb_size (db); ++i)
    {
      if (mulib_kvdb_at (db, i, &key, &kn, &v, &vn))
        return mulib_kronvm_fail (EINVAL);
      if (kn == profile_len && !memcmp (key, profile, profile_len))
        continue;
      const unsigned char *b = (const unsigned char *)v;
      if (kn != 8 || vn < 24)
        return mulib_kronvm_fail (EINVAL);
      uint64_t ticket = mulib_endian_load_be64 ((const unsigned char *)key);
      uint64_t id = mulib_endian_load_be64 (b),
               start = mulib_endian_load_be64 (b + 8),
               due = mulib_endian_load_be64 (b + 16);
      if (!id || id > ticket || ticket > vm->serial || start > INT64_MAX
          || due > INT64_MAX)
        return mulib_kronvm_fail (EINVAL);
      for (mulib_kronvm_task *q = vm->list; q; q = q->next)
        if (q->id == id)
          return mulib_kronvm_fail (EINVAL);
      mulib_kronvm_task *p;
      if (vm->jobs)
        p = mulib_kronvm_decode_job (b, vn, ticket, id, start, due);
      else
        {
          if (vn < 26 || b[vn - 1] || memchr (b + 24, 0, vn - 25))
            return mulib_kronvm_fail (EINVAL);
          p = mulib_kronvm_parse ((const char *)b + 24, (int64_t)start);
          if (p)
            {
              int64_t delta = (int64_t)due - p->notice.deadline;
              if (delta < 0 || (p->interval ? delta % p->interval : delta)
                  || (int64_t)due > p->end)
                {
                  mulib_kronvm_free (p);
                  return mulib_kronvm_fail (EINVAL);
                }
            }
        }
      if (!p)
        return -1;
      p->id = id;
      p->ticket = ticket;
      p->notice.deadline = (int64_t)due;
      mulib_kronvm_attach (vm, p);
    }
  return 0;
}
void
mulib_kronvm_close (mulib_kronvm_t *vm)
{
  if (!vm)
    return;
  mulib_evtsched_clear (&vm->queue);
  while (vm->list)
    {
      mulib_kronvm_task *p = vm->list;
      vm->list = p->next;
      mulib_kronvm_free (p);
    }
  if (vm->lock >= 0)
    close (vm->lock);
  free (vm->path);
  free (vm);
}
static int
mulib_kronvm_check_directory (const struct stat *st, int jobs)
{
  if (jobs && S_ISLNK (st->st_mode))
    return mulib_kronvm_fail (ELOOP);
  if (!S_ISDIR (st->st_mode))
    return mulib_kronvm_fail (ENOTDIR);
  if (st->st_uid != geteuid ()
      || (st->st_mode & (jobs ? S_IRWXG | S_IRWXO : S_IWGRP | S_IWOTH)))
    return mulib_kronvm_fail (EACCES);
  return 0;
}

/* KVDB opens by path, so this is a check for a stable trusted cache layout,
 * not protection against an owner replacing names between check and use. */
static int
mulib_kronvm_check_job_snapshot (const mulib_kronvm_t *vm, int create)
{
  struct stat st;
  if (lstat (vm->path, &st) < 0)
    return create && errno == ENOENT ? 0 : -1;
  if (S_ISLNK (st.st_mode))
    return mulib_kronvm_fail (ELOOP);
  if (!S_ISREG (st.st_mode))
    return mulib_kronvm_fail (EINVAL);
  if (st.st_uid != geteuid () || (st.st_mode & (S_IRWXG | S_IRWXO)))
    return mulib_kronvm_fail (EACCES);
  return 0;
}

static int
mulib_kronvm_open_as (const char *directory, mulib_kronvm_t **out, int jobs)
{
  if (!directory && !jobs)
    directory = "/var/run/kronvm";
  if (!out || !directory || *directory != '/')
    return mulib_kronvm_fail (EINVAL);
  size_t n = strlen (directory);
  /* A trailing slash would make lstat follow a directory symlink. Keep the
   * legacy path spelling but remove trailing slashes for the jobs profile. */
  if (jobs)
    while (n > 1 && directory[n - 1] == '/')
      --n;
  if (n > SIZE_MAX - sizeof "/events.kvdb")
    return mulib_kronvm_fail (EOVERFLOW);
  mulib_kronvm_t *vm = (mulib_kronvm_t *)calloc (1, sizeof *vm);
  if (!vm)
    return -1;
  vm->lock = -1;
  vm->jobs = jobs;
  mulib_kvdb_t *db = NULL;
  int rc;
  struct stat st;
  vm->path = (char *)malloc (n + sizeof "/events.kvdb");
  if (!vm->path)
    goto fail;
  memcpy (vm->path, directory, n);
  vm->path[n] = '\0';
  if (jobs && (lstat (vm->path, &st) < 0
               || mulib_kronvm_check_directory (&st, jobs) < 0))
    goto fail;
  if (!jobs && mkdir (directory, 0700) < 0 && errno != EEXIST)
    goto fail;
  int flags = O_RDONLY;
#ifdef O_DIRECTORY
  if (jobs)
    flags |= O_DIRECTORY;
#endif
#ifdef O_CLOEXEC
  if (jobs)
    flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
  if (jobs)
    flags |= O_NOFOLLOW;
#endif
  vm->lock = open (vm->path, flags);
  if (vm->lock < 0 || fcntl (vm->lock, F_SETFD, FD_CLOEXEC) < 0
      || fstat (vm->lock, &st) < 0)
    goto fail;
  if (mulib_kronvm_check_directory (&st, jobs) < 0)
    goto fail;
  if (flock (vm->lock, LOCK_EX | LOCK_NB) < 0)
    goto fail;
  if (jobs && n == 1)
    n = 0; /* Keep a single leading slash for the jobs profile. */
  memcpy (vm->path + n, "/events.kvdb", sizeof "/events.kvdb");
  if (jobs && mulib_kronvm_check_job_snapshot (vm, 1) < 0)
    goto fail;
  rc = mulib_kvdb_open (vm->path, 1, &db);
  if (rc < 0)
    {
      mulib_kronvm_db_error (rc);
      goto fail;
    }
  if (mulib_kvdb_dirty (db))
    {
      rc = mulib_kronvm_highwater (vm, db, 0);
      if (!rc)
        rc = mulib_kvdb_commit (db);
      if (rc < 0)
        {
          mulib_kronvm_db_error (rc);
          goto fail;
        }
    }
  else if (mulib_kronvm_restore (vm, db) < 0)
    goto fail;
  mulib_kvdb_close (db);
  *out = vm;
  return rc;
fail:
  {
    int error = errno;
    mulib_kvdb_close (db);
    mulib_kronvm_close (vm);
    return mulib_kronvm_fail (error);
  }
}
int
mulib_kronvm_open (const char *directory, mulib_kronvm_t **out)
{
  return mulib_kronvm_open_as (directory, out, 0) < 0 ? -1 : 0;
}
int
mulib_kronvm_open_jobs (const char *directory, mulib_kronvm_t **out)
{
  return mulib_kronvm_open_as (directory, out, 1);
}

static int
mulib_kronvm_write_task (mulib_kronvm_t *vm, mulib_kvdb_t *db,
                        const mulib_kronvm_task *put, int64_t due)
{
  const size_t prefix = vm->jobs ? MULIB_KRONVM_JOB_PREFIX : 24u;
  size_t n = vm->jobs ? put->job_size : strlen (put->source) + 1;
  if (n > SIZE_MAX - prefix)
    return MULIB_KVDB_EOVERFLOW;
  unsigned char *b = (unsigned char *)malloc (n + prefix);
  if (!b)
    return MULIB_KVDB_ENOMEM;
  mulib_endian_store_be64 (b, put->id);
  mulib_endian_store_be64 (b + 8, (uint64_t)put->start);
  mulib_endian_store_be64 (b + 16, (uint64_t)due);
  if (vm->jobs)
    {
      mulib_endian_store_be64 (b + 24, (uint64_t)put->first);
      mulib_endian_store_be64 (b + 32, (uint64_t)put->interval);
      mulib_endian_store_be64 (b + 40, (uint64_t)put->end);
      mulib_endian_store_be64 (b + 48, (uint64_t)put->signal);
      memcpy (b + prefix, put->job, n);
    }
  else
    memcpy (b + prefix, put->source, n);
  unsigned char key[8];
  mulib_endian_store_be64 (key, vm->serial + 1);
  int rc = mulib_kvdb_set (db, key, 8, b, n + prefix);
  free (b);
  return rc;
}

/* Each transition owns a disposable working snapshot. Publish it before
 * changing any intrusive notices so a failed commit is always retryable. */
static int
mulib_kronvm_change (mulib_kronvm_t *vm, mulib_kronvm_task *remove, int signal,
                     mulib_kronvm_task *put, int64_t due)
{
  mulib_kvdb_t *db = NULL;
  unsigned char key[8];
  if (put && vm->serial == UINT64_MAX)
    return mulib_kronvm_fail (EOVERFLOW);
  if (vm->jobs)
    {
      struct stat st;
      if (fstat (vm->lock, &st) < 0
          || mulib_kronvm_check_directory (&st, 1) < 0
          || mulib_kronvm_check_job_snapshot (vm, 0) < 0)
        return -1;
    }
  int rc = mulib_kvdb_open (vm->path, 0, &db);
  if (rc < 0)
    return mulib_kronvm_db_error (rc);
  if (vm->jobs)
    {
      /* At least reject a wrong profile or serial before writing. The host
       * must still keep other writers out of the locked cache directory. */
      uint64_t disk_serial;
      if (mulib_kronvm_read_highwater (vm, db, &disk_serial) < 0
          || disk_serial != vm->serial)
        {
          mulib_kvdb_close (db);
          return mulib_kronvm_fail (EINVAL);
        }
    }
  for (mulib_kronvm_task *p = vm->list; p && !rc; p = p->next)
    {
      if (p == remove || (signal && p->signal == signal))
        {
          mulib_endian_store_be64 (key, p->ticket);
          rc = mulib_kvdb_remove (db, key, 8);
        }
    }
  if (!rc && put)
    {
      rc = mulib_kronvm_write_task (vm, db, put, due);
      if (!rc)
        rc = mulib_kronvm_highwater (vm, db, vm->serial + 1);
    }
  if (!rc)
    rc = mulib_kvdb_commit (db);
  mulib_kvdb_close (db);
  if (rc < 0)
    return mulib_kronvm_db_error (rc);
  if (put)
    put->ticket = ++vm->serial;
  return rc;
}
static int
mulib_kronvm_admit (mulib_kronvm_t *vm, mulib_kronvm_task *p, uint64_t *id)
{
  p->id = vm->serial + 1;
  int rc = mulib_kronvm_change (vm, NULL, 0, p, p->notice.deadline);
  if (rc < 0)
    {
      int error = errno;
      mulib_kronvm_free (p);
      return mulib_kronvm_fail (error);
    }
  mulib_kronvm_attach (vm, p);
  *id = p->id;
  return rc;
}
int
mulib_kronvm_delegate (mulib_kronvm_t *vm, const char *source, int64_t now,
                       uint64_t *id)
{
  if (!vm || vm->jobs || !source || now < 0 || !id)
    return mulib_kronvm_fail (EINVAL);
  mulib_kronvm_task *p = mulib_kronvm_parse (source, now);
  return p ? mulib_kronvm_admit (vm, p, id) : -1;
}
static int
mulib_kronvm_job_schedule (const mulib_kronvm_schedule_t *schedule,
                           int64_t now, mulib_kronvm_task *p)
{
  if (!schedule || now < 0)
    return mulib_kronvm_fail (EINVAL);
  int64_t first, interval = 0, end = INT64_MAX;
  int signal = 0;
  switch (schedule->cadence)
    {
    case MULIB_KRONVM_ONCE:
      if (schedule->seconds < 0)
        return mulib_kronvm_fail (EINVAL);
      first = schedule->seconds;
      break;
    case MULIB_KRONVM_EVERY:
      interval = schedule->seconds;
      if (interval <= 0)
        return mulib_kronvm_fail (EINVAL);
      if (interval > INT64_MAX - now)
        return mulib_kronvm_fail (EOVERFLOW);
      first = now + interval;
      break;
    default:
      return mulib_kronvm_fail (EINVAL);
    }
  switch (schedule->lifetime)
    {
    case MULIB_KRONVM_PERSISTENT:
      if (schedule->limit != 0)
        return mulib_kronvm_fail (EINVAL);
      break;
    case MULIB_KRONVM_FOR:
      if (schedule->limit <= 0)
        return mulib_kronvm_fail (EINVAL);
      if (schedule->limit > INT64_MAX - now)
        return mulib_kronvm_fail (EOVERFLOW);
      end = now + schedule->limit;
      if (first > end)
        return mulib_kronvm_fail (EINVAL);
      break;
    case MULIB_KRONVM_UNTIL_SIGNAL:
      if (schedule->limit <= 0 || schedule->limit > INT32_MAX
          || schedule->limit > INT_MAX)
        return mulib_kronvm_fail (EINVAL);
      signal = (int)schedule->limit;
      break;
    default:
      return mulib_kronvm_fail (EINVAL);
    }
  p->interval = interval;
  p->end = end;
  p->start = now;
  p->signal = signal;
  p->first = p->notice.deadline = first;
  return 0;
}
int
mulib_kronvm_validate_job_schedule (const mulib_kronvm_schedule_t *schedule,
                                    int64_t now)
{
  mulib_kronvm_task task;
  return mulib_kronvm_job_schedule (schedule, now, &task);
}
int
mulib_kronvm_delegate_job (mulib_kronvm_t *vm,
                           const mulib_kronvm_schedule_t *schedule,
                           int64_t now, const void *data, size_t size,
                           uint64_t *id)
{
  if (!vm || !vm->jobs || !data || !size || !id)
    return mulib_kronvm_fail (EINVAL);
  if (size > MULIB_KRONVM_MAX_JOB_BYTES)
    return mulib_kronvm_fail (E2BIG);
  mulib_kronvm_task task = {0};
  if (mulib_kronvm_job_schedule (schedule, now, &task) < 0)
    return -1;
  mulib_kronvm_task *p = (mulib_kronvm_task *)malloc (sizeof *p);
  if (!p)
    return -1;
  *p = task;
  p->job = (unsigned char *)malloc (size);
  if (!p->job)
    {
      mulib_kronvm_free (p);
      return mulib_kronvm_fail (ENOMEM);
    }
  memcpy (p->job, data, size);
  p->job_size = size;
  p->notice.data = p;
  return mulib_kronvm_admit (vm, p, id);
}
static void
mulib_kronvm_detach (mulib_kronvm_t *vm, mulib_kronvm_task **link)
{
  mulib_kronvm_task *p = *link;
  mulib_evtsched_cancel (&vm->queue, &p->notice);
  *link = p->next;
}
int
mulib_kronvm_pull (mulib_kronvm_t *vm, uint64_t id)
{
  if (!vm)
    return mulib_kronvm_fail (EINVAL);
  for (mulib_kronvm_task **link = &vm->list; *link; link = &(*link)->next)
    {
      mulib_kronvm_task *p = *link;
      if (p->id == id)
        {
          int rc = mulib_kronvm_change (vm, p, 0, NULL, 0);
          if (rc >= 0)
            {
              mulib_kronvm_detach (vm, link);
              mulib_kronvm_free (p);
            }
          return rc;
        }
    }
  return mulib_kronvm_fail (ENOENT);
}
int
mulib_kronvm_signal (mulib_kronvm_t *vm, int number)
{
  if (!vm || number <= 0)
    return mulib_kronvm_fail (EINVAL);
#if INT_MAX > INT32_MAX
  if (vm->jobs && number > INT32_MAX)
    return mulib_kronvm_fail (EINVAL);
#endif
  int rc = mulib_kronvm_change (vm, NULL, number, NULL, 0);
  if (rc < 0)
    return -1;
  for (mulib_kronvm_task **link = &vm->list; *link;)
    {
      mulib_kronvm_task *p = *link;
      if (p->signal == number)
        {
          mulib_kronvm_detach (vm, link);
          mulib_kronvm_free (p);
        }
      else
        link = &p->next;
    }
  return rc;
}
int
mulib_kronvm_next (const mulib_kronvm_t *vm, int64_t *deadline)
{
  if (!vm || !deadline)
    return mulib_kronvm_fail (EINVAL);
  mulib_evtsched_notice_t *e = mulib_evtsched_peek (&vm->queue);
  if (!e)
    return 0;
  *deadline = e->deadline;
  return 1;
}
static void
mulib_kronvm_run (mulib_kronvm_task *p, mulib_kronvm_result_t *r)
{
  pid_t child = fork ();
  if (child < 0)
    {
      r->execution_errno = errno;
      return;
    }
  if (child == 0)
    {
      if (p->shell)
        execl ("/bin/sh", "sh", "-c", p->command, (char *)NULL);
      else
        execl (p->command, p->command, (char *)NULL);
      _exit (127);
    }
  r->launched = 1;
  while (waitpid (child, &r->wait_status, 0) < 0)
    {
      if (errno != EINTR)
        {
          r->execution_errno = errno;
          break;
        }
    }
}
static int
mulib_kronvm_consume (mulib_kronvm_t *vm, int64_t now,
                      mulib_kronvm_task **task, int64_t *due, int *rearmed)
{
  mulib_evtsched_notice_t *e = mulib_evtsched_peek (&vm->queue);
  if (!e || e->deadline > now)
    return 0;
  mulib_kronvm_task *p = (mulib_kronvm_task *)e->data;
  int64_t next = 0;
  if (p->interval && now <= p->end)
    {
      int64_t delay = p->interval - (now - e->deadline) % p->interval;
      if (delay <= p->end - now)
        next = now + delay;
    }
  int rc = mulib_kronvm_change (vm, p, 0, next ? p : NULL, next);
  if (rc < 0)
    return -1;
  *task = p;
  *due = e->deadline;
  *rearmed = next != 0;
  if (next)
    mulib_evtsched_schedule (&vm->queue, e, next);
  else
    {
      mulib_kronvm_task **link = &vm->list;
      while (*link != p)
        link = &(*link)->next;
      mulib_kronvm_detach (vm, link);
    }
  return 1 + rc;
}
int
mulib_kronvm_dispatch (mulib_kronvm_t *vm, int64_t now,
                       mulib_kronvm_result_t *result)
{
  if (!vm || vm->jobs || !result || now < 0)
    return mulib_kronvm_fail (EINVAL);
  mulib_kronvm_task *p;
  int64_t due;
  int rearmed;
  int rc = mulib_kronvm_consume (vm, now, &p, &due, &rearmed);
  if (rc <= 0)
    return rc;
  memset (result, 0, sizeof *result);
  result->id = p->id;
  result->deadline = due;
  if (now <= p->end)
    mulib_kronvm_run (p, result);
  if (!rearmed)
    mulib_kronvm_free (p);
  return rc;
}
int
mulib_kronvm_dispatch_job (mulib_kronvm_t *vm, int64_t now,
                           mulib_kronvm_job_fn hook, void *user, uint64_t *id,
                           int64_t *deadline)
{
  if (!vm || !vm->jobs || now < 0 || !hook || !id || !deadline)
    return mulib_kronvm_fail (EINVAL);
  mulib_kronvm_task *p;
  int64_t due;
  int rearmed;
  int rc = mulib_kronvm_consume (vm, now, &p, &due, &rearmed);
  if (rc <= 0)
    return rc;
  *id = p->id;
  *deadline = due;
  if (rc == 1 && now <= p->end)
    hook (user, p->id, due, p->job, p->job_size);
  if (!rearmed)
    mulib_kronvm_free (p);
  return rc;
}
#endif
