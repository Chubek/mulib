#define _GNU_SOURCE
#include "host_stage.h"
#include "linked_format.h"
#include "runner.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int on1x_archvirt_run_image(const void *bytes, size_t size) {
    const unsigned char *data = bytes;
    int fd = -1, status = 125;
    if (getuid() != geteuid() || getgid() != getegid()) {
        fputs("on1x VM launcher: unavailable in a set-ID process\n", stderr);
        return status;
    }
    if (!data || !size || size > ON1X_VM_GUEST_MAX) goto failed;
    fd = memfd_create("on1x-vm-guest", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd < 0) goto failed;
    for (size_t written = 0; written < size;) {
        ssize_t n = write(fd, data + written, size - written);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) goto failed;
        written += (size_t)n;
    }
    if (fchmod(fd, S_IRUSR) ||
        fcntl(fd, F_ADD_SEALS, F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL) < 0)
        goto failed;
    status = on1x_archvirt_run_sealed(fd);
    close(fd);
    return status;
failed:
    if (fd >= 0) close(fd);
    fputs("on1x VM launcher: could not stage a sealed guest\n", stderr);
    return status;
}
