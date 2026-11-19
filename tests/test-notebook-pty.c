#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#include "mulib-pty.h"
#include <assert.h>
#include <poll.h>
#include <stdio.h>
#include <sys/wait.h>
#include <string.h>

static void child(mulib_pty p)
{
    /* A stdio descriptor may have been vacant when the host acquired the pair. */
    if (p.slave != STDOUT_FILENO) {
        if (dup2(p.slave, STDOUT_FILENO) < 0 ||
            fcntl(STDOUT_FILENO, F_SETFD, FD_CLOEXEC) < 0)
            _exit(1);
        if (p.master == STDOUT_FILENO)
            p.master = -1;
        close(p.slave);
        p.slave = STDOUT_FILENO;
    }
    struct winsize size;
    if (!mulib_pty_child(&p) || p.master != -1 || p.slave != -1 ||
        tcgetpgrp(STDIN_FILENO) != getpgrp() ||
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) < 0 ||
        size.ws_row != 29 || size.ws_col != 88)
        _exit(1);
    for (int fd = 0; fd < 3; ++fd)
        if (fcntl(fd, F_GETFD) != 0 || !isatty(fd))
            _exit(2);
    if (write(STDOUT_FILENO, "ready", 5) != 5)
        _exit(3);
    _exit(0);
}

int main(void)
{
    mulib_pty p = MULIB_PTY_INIT;
    if (!mulib_pty_open(&p)) {
        /* Some CI sandboxes do not provide a writable devpts instance. */
        if (errno == ENOENT || errno == ENODEV || errno == ENXIO ||
            errno == EACCES || errno == EPERM) {
            puts("SKIP: PTY devpts unavailable");
            return 0;
        }
        assert(!"could not open a PTY");
    }
    assert(p.master >= 0 && p.slave >= 0 && p.master != p.slave);
    assert(fcntl(p.master, F_GETFD) == FD_CLOEXEC);
    assert(fcntl(p.slave, F_GETFD) == FD_CLOEXEC);
    assert(!mulib_pty_resize(p.master, 0, 88));
    assert(mulib_pty_resize(p.master, 29, 88));

    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0)
        child(p);
    close(p.slave);
    p.slave = -1;

    char text[5];
    size_t n = 0;
    while (n < sizeof text) {
        struct pollfd ready = {p.master, POLLIN, 0};
        int rc;
        do {
            rc = poll(&ready, 1, 2000);
        } while (rc < 0 && errno == EINTR);
        assert(rc == 1 && (ready.revents & POLLIN));
        ssize_t got = read(p.master, text + n, sizeof text - n);
        assert(got > 0);
        n += (size_t)got;
    }
    assert(memcmp(text, "ready", sizeof text) == 0);
    int status;
    assert(waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    mulib_pty_close(&p);
    assert(p.master == -1 && p.slave == -1);
    mulib_pty_close(&p);
    return 0;
}
