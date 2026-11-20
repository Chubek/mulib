#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "On1xRT-Dynamic.h"

#ifdef ON1X_RT_HAS_DYNAMIC
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <link.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

/* One private namespaced MuLIB implementation TU; applications may link MuLIB
 * independently without competing definitions from the runtime archive. */
#define MULIB_HASH_NO_INTRINSICS
#define MULIB_IMPL_SHA
#define mulib_sha_digest_size on1x_rt_mu_sha_digest_size
#define mulib_sha_init on1x_rt_mu_sha_init
#define mulib_sha_init_intrinsics on1x_rt_mu_sha_init_intrinsics
#define mulib_sha_update on1x_rt_mu_sha_update
#define mulib_sha_final on1x_rt_mu_sha_final
#define mulib_sha_squeeze on1x_rt_mu_sha_squeeze
#define mulib_sha on1x_rt_mu_sha
#define mulib_sha_intrinsics on1x_rt_mu_sha_intrinsics
#define mulib_sha0 on1x_rt_mu_sha0
#define mulib_sha1 on1x_rt_mu_sha1
#define mulib_sha224 on1x_rt_mu_sha224
#define mulib_sha256 on1x_rt_mu_sha256
#define mulib_sha384 on1x_rt_mu_sha384
#define mulib_sha512 on1x_rt_mu_sha512
#define mulib_sha512_224 on1x_rt_mu_sha512_224
#define mulib_sha512_256 on1x_rt_mu_sha512_256
#define mulib_sha3_224 on1x_rt_mu_sha3_224
#define mulib_sha3_256 on1x_rt_mu_sha3_256
#define mulib_sha3_384 on1x_rt_mu_sha3_384
#define mulib_sha3_512 on1x_rt_mu_sha3_512
#define mulib_shake128 on1x_rt_mu_shake128
#define mulib_shake256 on1x_rt_mu_shake256
#pragma GCC visibility push(hidden)
#include "mulib-sha.h"
#pragma GCC visibility pop

void on1x_rt_dynamic_image_release_(int fd) {
  if (fd >= 0)
    (void)close(fd);
}

On1xRT_Status on1x_rt_dynamic_snapshot_(const char *path, size_t budget,
                                       On1xRT_DynamicImage *image,
                                       const char **message) {
  static const char hex[] = "0123456789abcdef";
  unsigned char buffer[16384], digest[32];
  char source_path[64];
  mulib_sha_ctx hash;
  struct stat pinned_stat, source_stat;
  size_t remaining;
  int source = -1;
  On1xRT_Status status = ON1X_RT_ERROR;
  /* Inspect before opening for reading: never issue a normal open to a
   * device or FIFO named by the request. The procfd pins the checked inode. */
  const int pinned = open(path, O_PATH | O_CLOEXEC);
  if (pinned < 0) {
    *message = "cannot open native module image";
    return errno == ENOENT || errno == ENOTDIR ? ON1X_RT_NOT_FOUND
                                              : ON1X_RT_ERROR;
  }
  *message = "cannot inspect native module image";
  if (fstat(pinned, &pinned_stat) < 0)
    goto done;
  if (!S_ISREG(pinned_stat.st_mode) || pinned_stat.st_size <= 0) {
    *message = "native module image must be a nonempty regular file";
    status = ON1X_RT_INVALID_ARGUMENT;
    goto done;
  }
  if ((uintmax_t)pinned_stat.st_size > budget) {
    *message = "native module image exceeds the remaining byte budget";
    status = ON1X_RT_LIMIT;
    goto done;
  }
  const int length = snprintf(source_path, sizeof(source_path), "/proc/self/fd/%d",
                              pinned);
  if (length < 0 || (size_t)length >= sizeof(source_path))
    goto done;
  source = open(source_path, O_RDONLY | O_NONBLOCK | O_CLOEXEC | O_NOCTTY);
  if (source < 0) {
    *message = "cannot read the pinned native image through procfs";
    if (errno == ENOENT || errno == ENOTDIR)
      status = ON1X_RT_UNSUPPORTED;
    goto done;
  }
  if (fstat(source, &source_stat) < 0 ||
      source_stat.st_dev != pinned_stat.st_dev ||
      source_stat.st_ino != pinned_stat.st_ino ||
      source_stat.st_size != pinned_stat.st_size) {
    *message = "native module image changed during inspection";
    goto done;
  }

  image->fd = memfd_create("on1xrt-native", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  if (image->fd < 0) {
    *message = "cannot create a sealable native image";
    if (errno == ENOSYS || errno == EINVAL || errno == EPERM || errno == EACCES)
      status = ON1X_RT_UNSUPPORTED;
    goto done;
  }
  *message = "cannot copy native module image";
  remaining = (size_t)source_stat.st_size;
  while (remaining) {
    const size_t chunk = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
    const ssize_t read_size = read(source, buffer, chunk);
    if (read_size <= 0)
      goto done;
    for (size_t written = 0; written < (size_t)read_size;) {
      const ssize_t amount = write(image->fd, buffer + written,
                                   (size_t)read_size - written);
      if (amount <= 0)
        goto done;
      written += (size_t)amount;
    }
    remaining -= (size_t)read_size;
  }
  if (read(source, buffer, 1u) != 0)
    goto done;
  if (fcntl(image->fd, F_ADD_SEALS,
            F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL) < 0) {
    *message = "cannot seal native module image";
    status = ON1X_RT_UNSUPPORTED;
    goto done;
  }
  *message = "cannot hash the sealed native module image";
  if (!mulib_sha_init(&hash, MULIB_SHA256))
    goto done;
  remaining = (size_t)source_stat.st_size;
  for (size_t offset = 0; remaining;) {
    const size_t chunk = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
    const ssize_t amount = pread(image->fd, buffer, chunk, (off_t)offset);
    if (amount <= 0 || !mulib_sha_update(&hash, buffer, (size_t)amount))
      goto done;
    remaining -= (size_t)amount;
    offset += (size_t)amount;
  }
  if (pread(image->fd, buffer, 1u, source_stat.st_size) != 0 ||
      !mulib_sha_final(&hash, digest, sizeof(digest)))
    goto done;
  for (size_t i = 0; i < sizeof(digest); ++i) {
    image->sha256[2u * i] = hex[digest[i] >> 4u];
    image->sha256[2u * i + 1u] = hex[digest[i] & 15u];
  }
  image->sha256[64] = '\0';
  image->size = (size_t)source_stat.st_size;
  status = ON1X_RT_OK;

done:
  if (source >= 0)
    (void)close(source);
  (void)close(pinned);
  if (status != ON1X_RT_OK)
    on1x_rt_dynamic_image_release_(image->fd);
  return status;
}

On1xRT_Status on1x_rt_dynamic_open_(const On1xRT_DynamicImage *image,
                                   void **library, On1xRT_ModuleEntryFn *entry,
                                   const char **message) {
  static const char prefix[] = "/proc/self/fd/";
  char path[sizeof(prefix) + 64u * 4u * 3u + 32u];
  char *cursor = path + sizeof(prefix) - 1u;
  struct stat source_stat, path_stat;
  struct link_map *root = NULL;
  Dl_info info;
  void *symbol, *owner_map = NULL, *elf_symbol = NULL;
  int length;

  /* glibc can cache by the exact requested pathname even after dlclose (e.g.
   * NODELETE). Give each digest a distinct spelling of the procfd pathname,
   * so reusing an fd number cannot select cached code with different bytes. */
  memcpy(path, prefix, sizeof(prefix) - 1u);
  for (size_t i = 0; i < 64u; ++i) {
    const unsigned c = (unsigned char)image->sha256[i];
    const unsigned digit = c <= '9' ? c - '0' : c - 'a' + 10u;
    for (unsigned bit = 1u; bit <= 8u; bit <<= 1u) {
      *cursor++ = '.';
      *cursor++ = '/';
      if (digit & bit)
        *cursor++ = '/';
    }
  }
  length = snprintf(cursor, sizeof(path) - (size_t)(cursor - path), "%d", image->fd);
  if (length < 0 || (size_t)length >= sizeof(path) - (size_t)(cursor - path) ||
      fstat(image->fd, &source_stat) < 0 || stat(path, &path_stat) < 0 ||
      source_stat.st_dev != path_stat.st_dev || source_stat.st_ino != path_stat.st_ino) {
    *message = "procfs access to the sealed native image is unavailable";
    return ON1X_RT_UNSUPPORTED;
  }

  *library = dlopen(path, RTLD_NOW | RTLD_LOCAL);
  if (!*library) {
    *message = "OS loader could not open the authorized native image";
    return ON1X_RT_ERROR;
  }
  (void)dlerror();
  symbol = dlsym(*library, ON1X_RT_MODULE_ENTRY_SYMBOL_NAME);
  if (dlerror() || !symbol) {
    *message = "native image does not export the On1xRT ABI 1 entry point";
    return ON1X_RT_NOT_FOUND;
  }
  if (dlinfo(*library, RTLD_DI_LINKMAP, &root) != 0 || !root ||
      !dladdr1(symbol, &info, &owner_map, RTLD_DL_LINKMAP) || owner_map != root ||
      !dladdr1(symbol, &info, &elf_symbol, RTLD_DL_SYMENT) || !elf_symbol ||
      !info.dli_sname || strcmp(info.dli_sname, ON1X_RT_MODULE_ENTRY_SYMBOL_NAME) ||
      info.dli_saddr != symbol ||
      ELF64_ST_TYPE(((const ElfW(Sym) *)elf_symbol)->st_info) != STT_FUNC) {
    *message = "On1xRT ABI 1 entry must be a function defined by the image";
    return ON1X_RT_ABI_MISMATCH;
  }
  /* POSIX permits dlsym's result to be used as a function pointer. */
  _Static_assert(sizeof(symbol) == sizeof(*entry), "dlsym pointer size");
  memcpy(entry, &symbol, sizeof(symbol));
  return ON1X_RT_OK;
}

void on1x_rt_dynamic_close_(void *library) { (void)dlclose(library); }
#endif
