/* MuLIB PTY helpers (Linux/POSIX), C11 header-only.
 * SPDX-License-Identifier: MIT
 * Requires POSIX XSI declarations (e.g. -D_XOPEN_SOURCE=700 on Linux). */
#ifndef MULIB_PTY_H
#define MULIB_PTY_H
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <errno.h>

typedef struct {
    int master, slave;
} mulib_pty;
#define MULIB_PTY_INIT { -1, -1 }

/* Obtain the peer directly even across devpts mount namespaces. Fall back
 * only if the ioctl is unsupported. */
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
#else
    /* POSIX ptsname is not thread-safe; only used when no reentrant helper. */
    const char *path = ptsname(master);
    if (!path)
        return -1;
#endif
    int opened;
    do {
        opened = open(path, flags);
    } while (opened < 0 && errno == EINTR);
    return opened;
}

/* Initialize a pair set to MULIB_PTY_INIT. On failure leave it unchanged. */
static inline int mulib_pty_open(mulib_pty *p)
{
    if (!p || p->master != -1 || p->slave != -1) {
        errno = EINVAL;
        return 0;
    }
    int master;
    do {
        master = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
    } while (master < 0 && errno == EINTR);
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

/* Call only from a fresh single-threaded fork child, before exec. On failure
 * exit rather than continuing with partially redirected stdio. */
static inline int mulib_pty_child(mulib_pty *p)
{
    if (!p || p->slave < 0 || p->slave == p->master || setsid() < 0)
        return 0;
    int rc;
    do {
        rc = ioctl(p->slave, TIOCSCTTY, 0);
    } while (rc < 0 && errno == EINTR);
    if (rc < 0)
        return 0;
    if (p->master >= 0)
        close(p->master);
    p->master = -1;
    for (int i = 0; i < 3; ++i) {
        do {
            rc = dup2(p->slave, i);
        } while (rc < 0 && errno == EINTR);
        if (rc < 0)
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
