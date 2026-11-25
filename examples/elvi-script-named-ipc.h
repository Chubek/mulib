/* Optional Linux embedding example. Define _GNU_SOURCE before any system
 * header. The engine itself supports any host-authorized connected Unix fd.
 */
#ifndef ELVI_SCRIPT_NAMED_IPC_EXAMPLE_H
#define ELVI_SCRIPT_NAMED_IPC_EXAMPLE_H
#include "mulib-ipc-framed.h"
#if defined(__linux__) && defined(_GNU_SOURCE)
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

/* Return an owned, nonblocking, close-on-exec connection, or -1. The supplied
 * path approval must come from the host, not from the profile. Root and the
 * effective UID are trusted; two processes of the same UID are not isolated.
 * The receiver must bind inside a mode-0700 directory and bind its socket
 * with umask 077. This helper does not create or unlink files. */
static int elvi_script_example_connect(const char *requested,
                                       const char *approved, unsigned timeout_ms)
{
  struct sockaddr_un address = {0};
  char path[sizeof address.sun_path];
  char *part, *next;
  struct stat st, after;
  struct ucred peer;
  socklen_t length;
  const uid_t owner = geteuid();
  int fd = -1, dir = -1, success = 0, n, rc;
  int64_t deadline, now;
  size_t size;
  if (!requested || !approved || !timeout_ms || timeout_ms > 30000) return -1;
  size = strnlen(requested, sizeof path);
  if (size < 3 || size >= sizeof path || requested[0] != '/' ||
      strcmp(requested, approved) || mulib_ipc_deadline(timeout_ms, &deadline))
    return -1;
  memcpy(path, requested, size + 1);
  dir = open("/", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  if (dir < 0) return -1;
  part = path + 1;
  for (;;) {
    next = strchr(part, '/');
    if (next) *next++ = '\0';
    if (!*part || !strcmp(part, ".") || !strcmp(part, "..") || fstat(dir, &st) ||
        (st.st_uid != 0 && st.st_uid != owner) ||
        ((st.st_mode & 0022) && (st.st_uid != 0 || !(st.st_mode & S_ISVTX))))
      goto done;
    if (!next) break;
    rc = openat(dir, part, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (rc < 0) goto done;
    close(dir);
    dir = rc;
    part = next;
  }
  if (st.st_uid != owner || (st.st_mode & 0077) ||
      fstatat(dir, part, &st, AT_SYMLINK_NOFOLLOW) || !S_ISSOCK(st.st_mode) ||
      st.st_uid != owner || (st.st_mode & 0077))
    goto done;

  /* The kernel resolves the leaf relative to the pinned directory even if a
   * pathname ancestor has been renamed. The private directory protects the
   * leaf from other UIDs; reject a changed inode or a different peer UID. */
  address.sun_family = AF_UNIX;
  n = snprintf(address.sun_path, sizeof address.sun_path,
               "/proc/self/fd/%d/%s", dir, part);
  if (n < 0 || (size_t)n >= sizeof address.sun_path) goto done;
  fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  now = mulib_ipc_now_ms();
  if (fd < 0 || now < 0 || now >= deadline) goto done;
  rc = connect(fd, (const struct sockaddr *)&address,
               (socklen_t)(offsetof(struct sockaddr_un, sun_path) + (size_t)n + 1));
  if (rc < 0) {
    struct pollfd p = {fd, POLLOUT, 0};
    int error;
    /* EAGAIN on an AF_UNIX connect indicates a full accept queue. Fail
     * without replaying; the owner may initiate a fresh run. */
    if (errno != EINPROGRESS) goto done;
    do {
      now = mulib_ipc_now_ms();
      if (now < 0 || now >= deadline) goto done;
      rc = poll(&p, 1, (int)(deadline - now));
    } while (rc < 0 && errno == EINTR);
    length = sizeof error;
    if (rc <= 0 || !(p.revents & POLLOUT) ||
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) || error) goto done;
  }
  now = mulib_ipc_now_ms();
  length = sizeof peer;
  if (now < 0 || now >= deadline ||
      getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &peer, &length) ||
      length != sizeof peer || peer.uid != owner ||
      fstatat(dir, part, &after, AT_SYMLINK_NOFOLLOW) ||
      st.st_ino != after.st_ino || st.st_dev != after.st_dev ||
      after.st_uid != owner || (after.st_mode & 0077))
    goto done;
  success = 1;
done:
  close(dir);
  if (!success && fd >= 0) close(fd);
  return success ? fd : -1;
}
#endif
#endif
