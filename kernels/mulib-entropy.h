/** @file mulib-entropy.h
 * @brief Operating-system cryptographic entropy and secret-buffer utilities.
 * Define MULIB_IMPL_ENTROPY once. No deterministic PRNG fallback. */
#ifndef MULIB_ENTROPY_H
#define MULIB_ENTROPY_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/** Fill every byte, return 1; failure wipes the entire output and returns 0.
 * Linux uses blocking getrandom; other POSIX hosts use /dev/urandom.
 * NULL is allowed at length zero. Unsupported hosts fail. */
int mulib_entropy (void *output, size_t length);
void mulib_secret_wipe (void *data, size_t length);
/** Constant-operation-count comparison for equal public lengths. */
int mulib_secret_equal (const void *a, const void *b, size_t length);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ENTROPY) && !defined(MULIB_ENTROPY_IMPLEMENTED)
#define MULIB_ENTROPY_IMPLEMENTED
#include <errno.h>
#if defined(__linux__)
#include <sys/random.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <unistd.h>
#endif
void mulib_secret_wipe (void *p, size_t n)
{
  volatile unsigned char *v = (volatile unsigned char *)p;
  if (v) while (n--) *v++ = 0;
}
int mulib_secret_equal (const void *a, const void *b, size_t n)
{
  const unsigned char *x = (const unsigned char *)a, *y = (const unsigned char *)b;
  unsigned diff = 0;
  if ((!x || !y) && n) return 0;
  for (size_t i = 0; i < n; ++i) diff |= x[i] ^ y[i];
  return diff == 0;
}
int mulib_entropy (void *p, size_t n)
{
  unsigned char *out = (unsigned char *)p;
  size_t done = 0;
  if (!p && n) return 0;
#if defined(__linux__)
  while (done < n) {
    size_t take = n - done > 256 ? 256 : n - done;
    ssize_t rc = getrandom (out + done, take, 0);
    if (rc < 0 && errno == EINTR) continue;
    if (rc <= 0) break;
    done += (size_t)rc;
  }
#elif defined(__unix__) || defined(__APPLE__)
  if (n) {
    int fd = open ("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
      if (fcntl (fd, F_SETFD, FD_CLOEXEC) >= 0) while (done < n) {
        size_t take = n - done > 256 ? 256 : n - done;
        ssize_t rc = read (fd, out + done, take);
        if (rc < 0 && errno == EINTR) continue;
        if (rc <= 0) break;
        done += (size_t)rc;
      }
      (void)close (fd);
    }
  }
#else
  (void)out;
#endif
  if (done == n) return 1;
  mulib_secret_wipe (p, n); return 0;
}
#endif
