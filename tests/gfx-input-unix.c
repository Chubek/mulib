#include "mulib-glx-unix.h"
#include <assert.h>
#include <unistd.h>

int gfx_input_unix_smoke(void)
{
    int p[2], before;
    assert(!mulib_glx_unix_prepare_fd(-1) && errno == EINVAL);
    assert(pipe(p) == 0);
    before = fcntl(p[0], F_GETFL);
    assert(before >= 0);
    assert(mulib_glx_unix_prepare_fd(p[0]));
    assert(mulib_glx_unix_prepare_fd(p[0]));
    assert((fcntl(p[0], F_GETFL) & ~O_NONBLOCK) == (before & ~O_NONBLOCK));
    assert(fcntl(p[0], F_GETFL) & O_NONBLOCK);
    assert(fcntl(p[0], F_GETFD) & FD_CLOEXEC);
    assert(close(p[0]) == 0);
    assert(!mulib_glx_unix_prepare_fd(p[0]) && errno == EBADF);
    assert(close(p[1]) == 0);
    return 1;
}
