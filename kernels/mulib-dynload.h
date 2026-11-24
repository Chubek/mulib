/**
 * @file mulib-dynload.h
 * @brief Cross-platform shared-library loading, typed symbols and native names.
 * Define MULIB_IMPL_DYNLOAD once. Linux/other libdl systems link with -ldl.
 */
#ifndef MULIB_DYNLOAD_H
#define MULIB_DYNLOAD_H
#include <stddef.h>
/** @brief Mark shared-library exports (define EXPORT_SYMBOLS when building it). */
#if defined(_WIN32)
# if defined(MULIB_DYNLOAD_EXPORT_SYMBOLS)
#  define MULIB_DYNLOAD_EXPORT __declspec(dllexport)
# else
#  define MULIB_DYNLOAD_EXPORT __declspec(dllimport)
# endif
# define MULIB_DYNLOAD_CALL __cdecl
#elif defined(__GNUC__) || defined(__clang__)
# define MULIB_DYNLOAD_EXPORT __attribute__((visibility("default")))
# define MULIB_DYNLOAD_CALL
#else
# define MULIB_DYNLOAD_EXPORT
# define MULIB_DYNLOAD_CALL
#endif
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Owning library handle; initialize and do not copy. */
  typedef struct mulib_dynload
  {
    void *handle;
    int owned;
  } mulib_dynload_t;
#define MULIB_DYNLOAD_INIT { NULL, 0 }
  /** @brief Zero is success; negative results distinguish argument, loader and
   * state errors. */
  enum mulib_dynload_result
  {
    MULIB_DYNLOAD_OK = 0,
    MULIB_DYNLOAD_EINVAL = -1,
    MULIB_DYNLOAD_EIO = -2,
    MULIB_DYNLOAD_ENOTSUP = -3,
    MULIB_DYNLOAD_ESTATE = -4,
    MULIB_DYNLOAD_ERANGE = -5
  };
  /** @brief Optional POSIX binding/scope policy; zero means NOW | LOCAL.
   * Conflicting flags fail. Windows rejects LAZY and GLOBAL. */
  enum mulib_dynload_flags
  {
    MULIB_DYNLOAD_NOW = 1u, MULIB_DYNLOAD_LAZY = 2u,
    MULIB_DYNLOAD_LOCAL = 4u, MULIB_DYNLOAD_GLOBAL = 8u
  };
  /** @brief Load a library, or the current process when path is NULL.
   * Optional error buffer receives a terminated message (empty on success).
   * Failure leaves output unchanged; path resolution is the native loader's
   * policy.
   */
   int mulib_dynload_open (mulib_dynload_t *library, const char *path,
                            char *error, size_t capacity);
  /** @brief Open with explicit binding/scope flags; otherwise matches open. */
  int mulib_dynload_open_ex (mulib_dynload_t *library, const char *path,
                             unsigned flags, char *error, size_t capacity);
  /** @brief Open a named runtime and pin its module until process exit.
   * For runtimes with process-global caches or callbacks that cannot safely be
   * unloaded. Close still releases this handle's reference. Pinning is permanent
   * and shared by all users of the module. Requires a non-NULL path and native
   * RTLD_NODELETE or Windows module pinning; otherwise returns ENOTSUP.
   * Other contracts match open; no MuLIB global handle cache is maintained. */
  int mulib_dynload_open_pinned (mulib_dynload_t *library, const char *path,
                                 char *error, size_t capacity);
  /** @brief Look up a symbol, permitting a successful NULL-valued symbol on
   * POSIX. Output is unchanged on failure. It is borrowed until the library
   * closes. Function-pointer conversion follows the host ABI, not an ISO C
   * guarantee.
   */
   int mulib_dynload_symbol (const mulib_dynload_t *library, const char *name,
                             void **symbol, char *error, size_t capacity);
  /** @brief Copy a function address into an exactly typed function-pointer
   * object. Pass &function and sizeof function, not a void ** cast. Prototype
   * correctness remains the caller's responsibility. Transactional. */
  int mulib_dynload_function (const mulib_dynload_t *library, const char *name,
                              void *function, size_t function_size,
                              char *error, size_t capacity);
  /** @brief Convert a bare name to libNAME.so, libNAME.dylib or NAME.dll.
   * Rejects empty names, path separators and dot names; appends the platform
   * prefix/suffix unconditionally. ERANGE/invalid leaves output unchanged. */
  int mulib_dynload_native_name (const char *name, char *output, size_t capacity);
  /** @brief Release the handle; idempotent, retaining a handle if native close
   * fails. */
  int mulib_dynload_close (mulib_dynload_t *library, char *error,
                           size_t capacity);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_DYNLOAD_H */

#if defined(MULIB_IMPL_DYNLOAD) && !defined(MULIB_DYNLOAD_IMPLEMENTED)
#define MULIB_DYNLOAD_IMPLEMENTED
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <dlfcn.h>
#endif
#ifdef __cplusplus
extern "C"
{
#endif
  static void
  mulib_dynload_message_ (char *out, size_t cap, const char *message)
  {
    size_t n;
    if (!out || !cap)
      return;
    n = strlen (message);
    if (n >= cap)
      n = cap - 1;
    if (n)
      memcpy (out, message, n);
    out[n] = '\0';
  }
#ifdef _WIN32
  static void
  mulib_dynload_winerror_ (char *out, size_t cap)
  {
    char text[80];
    (void)snprintf (text, sizeof (text), "Windows loader error %lu",
                    (unsigned long)GetLastError ());
    mulib_dynload_message_ (out, cap, text);
  }
#endif
  int
  mulib_dynload_open (mulib_dynload_t *d, const char *path, char *error,
                      size_t cap)
  {
    return mulib_dynload_open_ex (d, path, 0, error, cap);
  }
  int
  mulib_dynload_open_ex (mulib_dynload_t *d, const char *path, unsigned flags,
                         char *error, size_t cap)
  {
    mulib_dynload_message_ (error, cap, "");
    if (!d || (flags & ~15u) || (flags & 3u) == 3u || (flags & 12u) == 12u)
      {
        mulib_dynload_message_ (error, cap, "invalid library output");
        return MULIB_DYNLOAD_EINVAL;
      }
    if (d->handle)
      {
        mulib_dynload_message_ (error, cap, "library already open");
        return MULIB_DYNLOAD_ESTATE;
      }
#if defined(_WIN32)
    {
      if (flags & (MULIB_DYNLOAD_LAZY | MULIB_DYNLOAD_GLOBAL))
        {
          mulib_dynload_message_ (error, cap, "loader policy unsupported");
          return MULIB_DYNLOAD_ENOTSUP;
        }
      HMODULE h = path ? LoadLibraryA (path) : GetModuleHandleA (NULL);
      if (!h)
        {
          mulib_dynload_winerror_ (error, cap);
          return MULIB_DYNLOAD_EIO;
        }
      d->handle = (void *)h;
      d->owned = path != NULL;
      return MULIB_DYNLOAD_OK;
    }
#elif defined(__unix__) || defined(__APPLE__)
  {
    int mode = (flags & MULIB_DYNLOAD_LAZY) ? RTLD_LAZY : RTLD_NOW;
    void *h;
    mode |= (flags & MULIB_DYNLOAD_GLOBAL) ? RTLD_GLOBAL : RTLD_LOCAL;
    h = dlopen (path, mode);
    if (!h)
      {
        const char *message = dlerror ();
        mulib_dynload_message_ (error, cap,
                                message ? message : "dlopen failed");
        return MULIB_DYNLOAD_EIO;
      }
    d->handle = h;
    d->owned = 1;
    return MULIB_DYNLOAD_OK;
  }
#else
  (void)path;
  mulib_dynload_message_ (error, cap, "dynamic loading unsupported");
  return MULIB_DYNLOAD_ENOTSUP;
#endif
  }
  int
  mulib_dynload_open_pinned (mulib_dynload_t *d, const char *path,
                             char *error, size_t cap)
  {
    mulib_dynload_message_ (error, cap, "");
    if (!d || !path)
      {
        mulib_dynload_message_ (error, cap, "invalid pinned library request");
        return MULIB_DYNLOAD_EINVAL;
      }
    if (d->handle)
      {
        mulib_dynload_message_ (error, cap, "library already open");
        return MULIB_DYNLOAD_ESTATE;
      }
#if defined(_WIN32)
    {
      mulib_dynload_t next = MULIB_DYNLOAD_INIT;
      HMODULE pinned;
      int result = mulib_dynload_open (&next, path, error, cap);
      if (result) return result;
      if (!GetModuleHandleExA (GET_MODULE_HANDLE_EX_FLAG_PIN |
                              GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                              (LPCSTR)next.handle, &pinned))
        {
          mulib_dynload_winerror_ (error, cap);
          (void)mulib_dynload_close (&next, NULL, 0);
          return MULIB_DYNLOAD_EIO;
        }
      *d = next;
      return MULIB_DYNLOAD_OK;
    }
#elif (defined(__unix__) || defined(__APPLE__)) && defined(RTLD_NODELETE)
    {
      void *h = dlopen (path, RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE);
      if (!h)
        {
          const char *message = dlerror ();
          mulib_dynload_message_ (error, cap, message ? message : "dlopen failed");
          return MULIB_DYNLOAD_EIO;
        }
      d->handle = h;
      d->owned = 1;
      return MULIB_DYNLOAD_OK;
    }
#else
    mulib_dynload_message_ (error, cap, "module pinning unsupported");
    return MULIB_DYNLOAD_ENOTSUP;
#endif
  }
  int
  mulib_dynload_symbol (const mulib_dynload_t *d, const char *name,
                        void **symbol, char *error, size_t cap)
  {
    mulib_dynload_message_ (error, cap, "");
    if (!d || !d->handle || !name || !*name || !symbol)
      {
        mulib_dynload_message_ (error, cap, "invalid symbol lookup");
        return MULIB_DYNLOAD_EINVAL;
      }
#if defined(_WIN32)
    {
      FARPROC address = GetProcAddress ((HMODULE)d->handle, name);
      if (!address)
        {
          mulib_dynload_winerror_ (error, cap);
          return MULIB_DYNLOAD_EIO;
        }
      if (sizeof (address) != sizeof (*symbol))
        {
          mulib_dynload_message_ (error, cap,
                                  "symbol representation unsupported");
          return MULIB_DYNLOAD_ENOTSUP;
        }
      memcpy (symbol, &address, sizeof (*symbol));
      return MULIB_DYNLOAD_OK;
    }
#elif defined(__unix__) || defined(__APPLE__)
  {
    void *address;
    const char *message;
    (void)dlerror ();
    address = dlsym (d->handle, name);
    message = dlerror ();
    if (message)
      {
        mulib_dynload_message_ (error, cap, message);
        return MULIB_DYNLOAD_EIO;
      }
    *symbol = address;
    return MULIB_DYNLOAD_OK;
  }
#else
  mulib_dynload_message_ (error, cap, "dynamic loading unsupported");
  return MULIB_DYNLOAD_ENOTSUP;
#endif
  }
  int
  mulib_dynload_function (const mulib_dynload_t *d, const char *name,
                          void *function, size_t size, char *error, size_t cap)
  {
    void *address = NULL;
    int result;
    mulib_dynload_message_ (error, cap, "");
    if (!function)
      {
        mulib_dynload_message_ (error, cap, "invalid function output");
        return MULIB_DYNLOAD_EINVAL;
      }
    if (size != sizeof (void (*)(void)) || size != sizeof address)
      {
        mulib_dynload_message_ (error, cap, "function representation unsupported");
        return MULIB_DYNLOAD_ENOTSUP;
      }
    result = mulib_dynload_symbol (d, name, &address, error, cap);
    if (!result) memcpy (function, &address, size);
    return result;
  }
  int
  mulib_dynload_native_name (const char *name, char *out, size_t cap)
  {
#if defined(_WIN32)
    const char *prefix = "", *suffix = ".dll";
#elif defined(__APPLE__)
    const char *prefix = "lib", *suffix = ".dylib";
#else
    const char *prefix = "lib", *suffix = ".so";
#endif
    size_t n, a = strlen (prefix), b = strlen (suffix);
    if (!name || !*name || !out || !strcmp (name, ".") || !strcmp (name, "..")
        || strpbrk (name, "/\\:")) return MULIB_DYNLOAD_EINVAL;
    n = strlen (name);
    if (n > (size_t)-1 - a - b - 1 || cap < a + n + b + 1)
      return MULIB_DYNLOAD_ERANGE;
    /* Move the name first, permitting output == name. */
    memmove (out + a, name, n);
    memcpy (out, prefix, a); memcpy (out + a + n, suffix, b + 1);
    return MULIB_DYNLOAD_OK;
  }
  int
  mulib_dynload_close (mulib_dynload_t *d, char *error, size_t cap)
  {
    mulib_dynload_message_ (error, cap, "");
    if (!d)
      {
        mulib_dynload_message_ (error, cap, "invalid library");
        return MULIB_DYNLOAD_EINVAL;
      }
    if (!d->handle)
      return MULIB_DYNLOAD_OK;
#if defined(_WIN32)
    if (d->owned && !FreeLibrary ((HMODULE)d->handle))
      {
        mulib_dynload_winerror_ (error, cap);
        return MULIB_DYNLOAD_EIO;
      }
#elif defined(__unix__) || defined(__APPLE__)
  if (d->owned && dlclose (d->handle) != 0)
    {
      const char *message = dlerror ();
      mulib_dynload_message_ (error, cap,
                              message ? message : "dlclose failed");
      return MULIB_DYNLOAD_EIO;
    }
#else
  mulib_dynload_message_ (error, cap, "dynamic loading unsupported");
  return MULIB_DYNLOAD_ENOTSUP;
#endif
    d->handle = NULL;
    d->owned = 0;
    return MULIB_DYNLOAD_OK;
  }
#ifdef __cplusplus
}
#endif
#endif /* implementation */
