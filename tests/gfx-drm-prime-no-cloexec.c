/* Simulate libc feature visibility without O_CLOEXEC. No ioctl may be issued. */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#ifdef O_CLOEXEC
#undef O_CLOEXEC
#endif
static int unexpected_ioctl(int fd, unsigned long op, void *arg)
{
    (void)fd; (void)op; (void)arg;
    assert(!"PRIME export without a native CLOEXEC flag must not issue ioctl");
    errno = ENOTTY;
    return -1;
}
#define MULIB_DRM_IOCTL(fd, op, arg) unexpected_ioctl((fd), (op), (arg))
#include "mulib-drm-prime.h"
int main(void)
{
#if MULIB_GFX_DRM_UAPI
    int fd = 99;
    assert(mulib_drm_prime_export(123, 10, &fd) == -1);
    assert(errno == ENOTSUP && fd == 99);
    puts("gfx-drm: missing native O_CLOEXEC returns ENOTSUP without ioctl");
#else
    /* Keep the local test hook referenced on a no-UAPI target. */
    (void)unexpected_ioctl;
    puts("gfx-drm: PRIME fixture skipped (no UAPI)");
#endif
    return 0;
}
