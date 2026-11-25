#ifndef ON1X_SEALED_NATIVE_H
#define ON1X_SEALED_NATIVE_H

/* Private Linux/glibc native-loader helpers shared with On1xRT. Callers
 * define _GNU_SOURCE early and own image admission and loader lifetimes. */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define ON1X_SEALED_NATIVE_PATH_CAP (sizeof("/proc/self/fd/") + 64u * 4u * 3u + 32u)

static inline int on1x_sealed_native_create_(const char *label) {
  return memfd_create(label, MFD_CLOEXEC | MFD_ALLOW_SEALING);
}

static inline int on1x_sealed_native_seal_(int fd) {
  return fcntl(fd, F_ADD_SEALS,
               F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL);
}

static inline int on1x_sealed_native_proc_path_(
    int fd, const char sha256[65], char path[ON1X_SEALED_NATIVE_PATH_CAP]) {
  static const char prefix[] = "/proc/self/fd/";
  char *cursor = path + sizeof prefix - 1u;
  struct stat fd_stat, path_stat;

  /* glibc can cache a particular pathname beyond dlclose (e.g. NODELETE).
   * Give different image digests distinct spellings, even if an fd number
   * is eventually reused. Keep the sealed fd open while the image is live. */
  memcpy(path, prefix, sizeof prefix - 1u);
  for (size_t i = 0; i < 64u; ++i) {
    unsigned c = (unsigned char)sha256[i], digit;
    if (c >= '0' && c <= '9')
      digit = c - '0';
    else if (c >= 'a' && c <= 'f')
      digit = c - 'a' + 10u;
    else
      return 0;
    for (unsigned bit = 1u; bit <= 8u; bit <<= 1u) {
      *cursor++ = '.';
      *cursor++ = '/';
      if (digit & bit) *cursor++ = '/';
    }
  }
  size_t capacity = ON1X_SEALED_NATIVE_PATH_CAP - (size_t)(cursor - path);
  int n = snprintf(cursor, capacity, "%d", fd);
  return n >= 0 && (size_t)n < capacity &&
      fstat(fd, &fd_stat) == 0 && stat(path, &path_stat) == 0 &&
      fd_stat.st_dev == path_stat.st_dev && fd_stat.st_ino == path_stat.st_ino;
}

#endif
