/* MuLIB graphical extension: glx-unix. C11, header-only. */
#ifndef MULIB_GLX_UNIX_H
#define MULIB_GLX_UNIX_H
#include "mulib-glx-window.h"
#include "mulib-glx-eventloop.h"
#include "mulib-glx-backend-drm.h"
#include "mulib-glx-backend-wayland.h"
#include "mulib-glx-backend-x11.h"
#include "mulib-input-seat.h"
#include <fcntl.h>

/* Explicit borrowed-fd preparation only: never discovers/opens devices.
 * Preserve unrelated flags. Not an atomic CLOEXEC acquisition: callers must
 * serialize this with fork/exec, or acquire descriptors atomically themselves.
 * If F_SETFL fails after F_SETFD, FD_CLOEXEC may already have been set. */
static inline int mulib_glx_unix_prepare_fd(int fd)
{
    int flags, fdflags;
    if (fd < 0) { errno = EINVAL; return 0; }
    flags = fcntl(fd, F_GETFL);
    if (flags < 0) return 0;
    fdflags = fcntl(fd, F_GETFD);
    if (fdflags < 0) return 0;
    if (!(fdflags & FD_CLOEXEC) && fcntl(fd, F_SETFD, fdflags | FD_CLOEXEC) < 0)
        return 0;
    if (!(flags & O_NONBLOCK) && fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
        return 0;
    return 1;
}
#endif
