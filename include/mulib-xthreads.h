/**
 * @file mulib-xthreads.h
 * @brief Portable native threads, mutexes, conditions, once and thread-local keys.
 * Define MULIB_IMPL_XTHREADS once. POSIX users link with -pthread; Windows
 * requires Vista or later. See manual/mulib-xthreads.md.
 */
#ifndef MULIB_XTHREADS_H
#define MULIB_XTHREADS_H
#include <stdint.h>
#include <stddef.h>
#if defined(_WIN32)
#include <windows.h>
typedef HANDLE mulib_xthread_t;
typedef SRWLOCK mulib_xmutex_t;
typedef CONDITION_VARIABLE mulib_xcond_t;
typedef INIT_ONCE mulib_xonce_t;
typedef DWORD mulib_xtss_t;
#define MULIB_XMUTEX_INIT SRWLOCK_INIT
#define MULIB_XCOND_INIT CONDITION_VARIABLE_INIT
#define MULIB_XONCE_INIT INIT_ONCE_STATIC_INIT
#elif defined(__unix__) || defined(__APPLE__)
#include <pthread.h>
typedef pthread_t mulib_xthread_t;
typedef pthread_mutex_t mulib_xmutex_t;
typedef pthread_cond_t mulib_xcond_t;
typedef pthread_once_t mulib_xonce_t;
typedef pthread_key_t mulib_xtss_t;
#define MULIB_XMUTEX_INIT PTHREAD_MUTEX_INITIALIZER
#define MULIB_XCOND_INIT PTHREAD_COND_INITIALIZER
#define MULIB_XONCE_INIT PTHREAD_ONCE_INIT
#else
#error "mulib-xthreads requires Windows or POSIX threads"
#endif
#define MULIB_XWAIT_FOREVER UINT64_MAX
#ifdef __cplusplus
extern "C" {
#endif
enum {
  MULIB_XTHREAD_OK = 0, MULIB_XTHREAD_INVALID = -1,
  MULIB_XTHREAD_NOMEM = -2, MULIB_XTHREAD_SYSTEM = -3,
  MULIB_XTHREAD_BUSY = -4, MULIB_XTHREAD_TIMEOUT = -5
};
typedef int (*mulib_xthread_fn)(void *);
/** Start a native thread. Argument remains caller-owned. */
int mulib_xthread_create (mulib_xthread_t *out, mulib_xthread_fn, void *);
/** Join exactly once, optionally retrieving the callback's integer result. */
int mulib_xthread_join (mulib_xthread_t, int *result);
/** Release a thread handle without waiting; mutually exclusive with join. */
int mulib_xthread_detach (mulib_xthread_t);
/** Yield the native processor. */
void mulib_xthread_yield (void);
/** Sleep for a relative duration in milliseconds. */
void mulib_xthread_sleep (uint64_t milliseconds);
/** Initialize a nonrecursive mutex; alternatively use MULIB_XMUTEX_INIT. */
int mulib_xmutex_init (mulib_xmutex_t *);
/** Destroy an unlocked mutex with no users. */
void mulib_xmutex_destroy (mulib_xmutex_t *);
/** Acquire a mutex; recursive acquisition is not supported. */
int mulib_xmutex_lock (mulib_xmutex_t *);
/** Try once; return BUSY if held. */
int mulib_xmutex_trylock (mulib_xmutex_t *);
/** Release a mutex owned by the calling native thread. */
int mulib_xmutex_unlock (mulib_xmutex_t *);
/** Initialize a condition; alternatively use MULIB_XCOND_INIT. */
int mulib_xcond_init (mulib_xcond_t *);
/** Destroy a condition after all waiters have returned. */
void mulib_xcond_destroy (mulib_xcond_t *);
/** Atomically release/reacquire mutex. Spurious wakeups are permitted. */
int mulib_xcond_wait (mulib_xcond_t *, mulib_xmutex_t *);
/** Wait until an absolute UTC millisecond deadline; FOREVER waits indefinitely. */
int mulib_xcond_wait_until (mulib_xcond_t *, mulib_xmutex_t *, uint64_t deadline);
/** Wake at least one waiter. */
int mulib_xcond_signal (mulib_xcond_t *);
/** Wake all waiters. */
int mulib_xcond_broadcast (mulib_xcond_t *);
/** Run a callback exactly once; callback must not recurse on the same flag. */
int mulib_xcall_once (mulib_xonce_t *, void (*callback)(void));
/** Allocate a TLS key (no automatic destructor). */
int mulib_xtss_create (mulib_xtss_t *);
/** Release a key after all threads stop using it; values are not freed. */
void mulib_xtss_delete (mulib_xtss_t);
/** Set the calling thread's borrowed value. */
int mulib_xtss_set (mulib_xtss_t, void *);
/** Read the calling thread's value, initially NULL. */
void *mulib_xtss_get (mulib_xtss_t);
/** Current UTC time in milliseconds. Deadlines follow wall-clock adjustments. */
uint64_t mulib_xclock_ms (void);
/** Convert a relative timeout to a saturating absolute deadline. */
uint64_t mulib_xdeadline (uint64_t timeout_ms);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_XTHREADS) && !defined(MULIB_XTHREADS_IMPLEMENTED)
#define MULIB_XTHREADS_IMPLEMENTED
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#if defined(_WIN32)
#include <process.h>
#else
#include <sched.h>
#endif
typedef struct mulib_xthread_start_ { mulib_xthread_fn fn; void *arg; } mulib_xthread_start_;
#if defined(_WIN32)
static unsigned __stdcall mulib_xthread_entry_ (void *v)
#else
static void *mulib_xthread_entry_ (void *v)
#endif
{
  mulib_xthread_start_ start = *(mulib_xthread_start_ *)v;
  int result;
  free (v);
  result = start.fn (start.arg);
#if defined(_WIN32)
  return (unsigned)result;
#else
  return (void *)(intptr_t)result;
#endif
}
int mulib_xthread_create (mulib_xthread_t *out, mulib_xthread_fn fn, void *arg)
{
  mulib_xthread_start_ *s;
  if (!out || !fn) return MULIB_XTHREAD_INVALID;
  s = (mulib_xthread_start_ *)malloc (sizeof (*s));
  if (!s) return MULIB_XTHREAD_NOMEM;
  s->fn = fn; s->arg = arg;
#if defined(_WIN32)
  *out = (HANDLE)_beginthreadex (NULL, 0, mulib_xthread_entry_, s, 0, NULL);
  if (!*out) { free (s); return MULIB_XTHREAD_SYSTEM; }
#else
  if (pthread_create (out, NULL, mulib_xthread_entry_, s)) {
    free (s); return MULIB_XTHREAD_SYSTEM;
  }
#endif
  return 0;
}
int mulib_xthread_join (mulib_xthread_t t, int *result)
{
#if defined(_WIN32)
  DWORD code;
  if (WaitForSingleObject (t, INFINITE) != WAIT_OBJECT_0
      || !GetExitCodeThread (t, &code)) return MULIB_XTHREAD_SYSTEM;
  if (result) *result = (int)code;
  return CloseHandle (t) ? 0 : MULIB_XTHREAD_SYSTEM;
#else
  void *code;
  if (pthread_join (t, &code)) return MULIB_XTHREAD_SYSTEM;
  if (result) *result = (int)(intptr_t)code;
  return 0;
#endif
}
int mulib_xthread_detach (mulib_xthread_t t)
{
#if defined(_WIN32)
  return CloseHandle (t) ? 0 : MULIB_XTHREAD_SYSTEM;
#else
  return pthread_detach (t) ? MULIB_XTHREAD_SYSTEM : 0;
#endif
}
void mulib_xthread_yield (void)
{
#if defined(_WIN32)
  (void)SwitchToThread ();
#else
  (void)sched_yield ();
#endif
}
void mulib_xthread_sleep (uint64_t ms)
{
  /* Bounded chunks avoid time_t/DWORD overflow and Windows' INFINITE value. */
  while (ms) {
    uint64_t chunk = ms > 86400000 ? 86400000 : ms;
#if defined(_WIN32)
    Sleep ((DWORD)chunk);
#else
    /* pthread timed waits remain available in strict C11 without requiring
     * feature-test macros before the application's first system header. */
    mulib_xmutex_t mutex = MULIB_XMUTEX_INIT;
    mulib_xcond_t condition = MULIB_XCOND_INIT;
    uint64_t deadline = mulib_xdeadline (chunk);
    (void)mulib_xmutex_lock (&mutex);
    while (mulib_xclock_ms () < deadline) {
      int rc = mulib_xcond_wait_until (&condition, &mutex, deadline);
      if (rc && rc != MULIB_XTHREAD_TIMEOUT) break;
    }
    (void)mulib_xmutex_unlock (&mutex);
    mulib_xcond_destroy (&condition); mulib_xmutex_destroy (&mutex);
#endif
    ms -= chunk;
  }
}
int mulib_xmutex_init (mulib_xmutex_t *m)
{
  if (!m) return MULIB_XTHREAD_INVALID;
#if defined(_WIN32)
  InitializeSRWLock (m); return 0;
#else
  return pthread_mutex_init (m, NULL) ? MULIB_XTHREAD_SYSTEM : 0;
#endif
}
void mulib_xmutex_destroy (mulib_xmutex_t *m)
{
#if defined(_WIN32)
  (void)m;
#else
  if (m) (void)pthread_mutex_destroy (m);
#endif
}
int mulib_xmutex_lock (mulib_xmutex_t *m)
{
  if (!m) return MULIB_XTHREAD_INVALID;
#if defined(_WIN32)
  AcquireSRWLockExclusive (m); return 0;
#else
  return pthread_mutex_lock (m) ? MULIB_XTHREAD_SYSTEM : 0;
#endif
}
int mulib_xmutex_trylock (mulib_xmutex_t *m)
{
  if (!m) return MULIB_XTHREAD_INVALID;
#if defined(_WIN32)
  return TryAcquireSRWLockExclusive (m) ? 0 : MULIB_XTHREAD_BUSY;
#else
  int rc = pthread_mutex_trylock (m);
  return rc == EBUSY ? MULIB_XTHREAD_BUSY : rc ? MULIB_XTHREAD_SYSTEM : 0;
#endif
}
int mulib_xmutex_unlock (mulib_xmutex_t *m)
{
  if (!m) return MULIB_XTHREAD_INVALID;
#if defined(_WIN32)
  ReleaseSRWLockExclusive (m); return 0;
#else
  return pthread_mutex_unlock (m) ? MULIB_XTHREAD_SYSTEM : 0;
#endif
}
int mulib_xcond_init (mulib_xcond_t *c)
{
  if (!c) return MULIB_XTHREAD_INVALID;
#if defined(_WIN32)
  InitializeConditionVariable (c); return 0;
#else
  return pthread_cond_init (c, NULL) ? MULIB_XTHREAD_SYSTEM : 0;
#endif
}
void mulib_xcond_destroy (mulib_xcond_t *c)
{
#if defined(_WIN32)
  (void)c;
#else
  if (c) (void)pthread_cond_destroy (c);
#endif
}
uint64_t mulib_xclock_ms (void)
{
#if defined(_WIN32)
  FILETIME ft;
  ULARGE_INTEGER n;
  GetSystemTimeAsFileTime (&ft); n.LowPart = ft.dwLowDateTime; n.HighPart = ft.dwHighDateTime;
  return n.QuadPart / 10000 - UINT64_C(11644473600000);
#else
  struct timespec t;
  (void)timespec_get (&t, TIME_UTC);
  return (uint64_t)t.tv_sec * 1000 + (uint64_t)t.tv_nsec / 1000000;
#endif
}
uint64_t mulib_xdeadline (uint64_t ms)
{
  uint64_t now = mulib_xclock_ms ();
  return ms >= UINT64_MAX - now ? UINT64_MAX : now + ms;
}
int mulib_xcond_wait_until (mulib_xcond_t *c, mulib_xmutex_t *m, uint64_t deadline)
{
  if (!c || !m) return MULIB_XTHREAD_INVALID;
#if defined(_WIN32)
  DWORD delay = INFINITE;
  if (deadline != UINT64_MAX) {
    uint64_t now = mulib_xclock_ms (), left = deadline > now ? deadline - now : 0;
    delay = left >= INFINITE ? INFINITE - 1 : (DWORD)left;
  }
  if (SleepConditionVariableSRW (c, m, delay, 0)) return 0;
  if (GetLastError () != ERROR_TIMEOUT) return MULIB_XTHREAD_SYSTEM;
  return deadline > mulib_xclock_ms () ? 0 : MULIB_XTHREAD_TIMEOUT;
#else
  int rc;
  if (deadline == UINT64_MAX) rc = pthread_cond_wait (c, m);
  else {
    struct timespec t;
    /* Clamp to representable time_t, allowing a spurious early wake on 32-bit. */
    uint64_t seconds = deadline / 1000;
    if (sizeof (time_t) <= 4 && seconds > INT32_MAX) seconds = INT32_MAX;
    t.tv_sec = (time_t)seconds; t.tv_nsec = (long)(deadline % 1000) * 1000000L;
    rc = pthread_cond_timedwait (c, m, &t);
  }
  return rc == ETIMEDOUT ? MULIB_XTHREAD_TIMEOUT : rc ? MULIB_XTHREAD_SYSTEM : 0;
#endif
}
int mulib_xcond_wait (mulib_xcond_t *c, mulib_xmutex_t *m)
{ return mulib_xcond_wait_until (c, m, UINT64_MAX); }
int mulib_xcond_signal (mulib_xcond_t *c)
{
  if (!c) return MULIB_XTHREAD_INVALID;
#if defined(_WIN32)
  WakeConditionVariable (c); return 0;
#else
  return pthread_cond_signal (c) ? MULIB_XTHREAD_SYSTEM : 0;
#endif
}
int mulib_xcond_broadcast (mulib_xcond_t *c)
{
  if (!c) return MULIB_XTHREAD_INVALID;
#if defined(_WIN32)
  WakeAllConditionVariable (c); return 0;
#else
  return pthread_cond_broadcast (c) ? MULIB_XTHREAD_SYSTEM : 0;
#endif
}
#if defined(_WIN32)
typedef struct mulib_xonce_call_ { void (*fn)(void); } mulib_xonce_call_;
static BOOL CALLBACK mulib_xonce_entry_ (PINIT_ONCE once, PVOID arg, PVOID *context)
{
  (void)once; (void)context; ((mulib_xonce_call_ *)arg)->fn (); return TRUE;
}
#endif
int mulib_xcall_once (mulib_xonce_t *once, void (*fn)(void))
{
  if (!once || !fn) return MULIB_XTHREAD_INVALID;
#if defined(_WIN32)
  mulib_xonce_call_ call; call.fn = fn;
  return InitOnceExecuteOnce (once, mulib_xonce_entry_, &call, NULL) ? 0 : MULIB_XTHREAD_SYSTEM;
#else
  return pthread_once (once, fn) ? MULIB_XTHREAD_SYSTEM : 0;
#endif
}
int mulib_xtss_create (mulib_xtss_t *key)
{
  if (!key) return MULIB_XTHREAD_INVALID;
#if defined(_WIN32)
  *key = TlsAlloc (); return *key == TLS_OUT_OF_INDEXES ? MULIB_XTHREAD_SYSTEM : 0;
#else
  return pthread_key_create (key, NULL) ? MULIB_XTHREAD_SYSTEM : 0;
#endif
}
void mulib_xtss_delete (mulib_xtss_t key)
{
#if defined(_WIN32)
  (void)TlsFree (key);
#else
  (void)pthread_key_delete (key);
#endif
}
int mulib_xtss_set (mulib_xtss_t key, void *value)
{
#if defined(_WIN32)
  return TlsSetValue (key, value) ? 0 : MULIB_XTHREAD_SYSTEM;
#else
  return pthread_setspecific (key, value) ? MULIB_XTHREAD_SYSTEM : 0;
#endif
}
void *mulib_xtss_get (mulib_xtss_t key)
{
#if defined(_WIN32)
  return TlsGetValue (key);
#else
  return pthread_getspecific (key);
#endif
}
#endif
