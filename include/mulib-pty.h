/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_PTY_H
#define MULIB_PTY_H
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <errno.h>
/* Requires POSIX XSI declarations e.g. -D_XOPEN_SOURCE=700. */
typedef struct {
    int master, slave;
} mulib_pty;
#define MULIB_PTY_INIT { -1, -1 }

/* Linux: obtain the paired slave directly from the master, even across
 * different devpts mount namespaces. Fall back only on missing ioctl support.
 * The last-resort POSIX ptsname() branch requires a single-threaded host. */
static inline int mulib_pty_open_peer_(int master)
{
    const int flags = O_RDWR | O_NOCTTY | O_CLOEXEC;
#if defined(__linux__) && defined(TIOCGPTPEER)
    int peer;
    do {
        peer = ioctl(master, TIOCGPTPEER, flags);
    } while (peer < 0 && errno == EINTR);
    if (peer >= 0 || (errno != EINVAL && errno != ENOTTY && errno != ENOSYS))
        return peer;
#endif
#if defined(__linux__) && defined(_GNU_SOURCE)
    char path[256];
    int error = ptsname_r(master, path, sizeof path);
    if (error) {
        if (error > 0)
            errno = error;
        return -1;
    }
    return open(path, flags);
#else
    const char *path = ptsname(master);
    return path ? open(path, flags) : -1;
#endif
}

/* Initialize an empty pair. On failure, leave p unchanged. */
static inline int mulib_pty_open(mulib_pty *p)
{
    if (!p) {
        errno = EINVAL;
        return 0;
    }
    int master = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (master < 0)
        return 0;
    int slave = -1;
    if (grantpt(master) == 0 && unlockpt(master) == 0)
        slave = mulib_pty_open_peer_(master);
    if (slave < 0) {
        int error = errno;
        close(master);
        errno = error;
        return 0;
    }
    *p = (mulib_pty){master, slave};
    return 1;
}

static inline int mulib_pty_resize(int fd, unsigned rows, unsigned cols)
{
    if (fd < 0 || !rows || !cols || rows > 65535 || cols > 65535) {
        errno = EINVAL;
        return 0;
    }
    struct winsize w = {0};
    w.ws_row = (unsigned short)rows;
    w.ws_col = (unsigned short)cols;
    int rc;
    do {
        rc = ioctl(fd, TIOCSWINSZ, &w);
    } while (rc < 0 && errno == EINTR);
    return rc == 0;
}

/* In a freshly forked single-threaded child only; sets controlling tty and
 * stdio. On failure the child should exit, not continue with partial stdio. */
static inline int mulib_pty_child(mulib_pty *p)
{
    if (!p || p->slave < 0 || p->slave == p->master || setsid() < 0)
        return 0;
    if (ioctl(p->slave, TIOCSCTTY, 0) < 0)
        return 0;
    if (p->master >= 0)
        close(p->master);
    p->master = -1;
    for (int i = 0; i < 3; ++i) {
        if (dup2(p->slave, i) < 0)
            return 0;
        if (p->slave == i) {
            int flags = fcntl(i, F_GETFD);
            if (flags < 0 || fcntl(i, F_SETFD, flags & ~FD_CLOEXEC) < 0)
                return 0;
        }
    }
    if (p->slave > 2)
        close(p->slave);
    p->slave = -1;
    return 1;
}

static inline void mulib_pty_close(mulib_pty *p)
{
    if (!p)
        return;
    if (p->master >= 0)
        close(p->master);
    if (p->slave >= 0 && p->slave != p->master)
        close(p->slave);
    p->master = p->slave = -1;
}
#endif
