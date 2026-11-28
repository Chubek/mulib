#define _GNU_SOURCE
#include "host_stage.h"
#include "linked_format.h"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int read_at(int fd, void *output, size_t count, off_t offset) {
    unsigned char *out = output;
    while (count) {
        ssize_t n = pread(fd, out, count, offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        out += n;
        count -= (size_t)n;
        offset += n;
    }
    return 0;
}

int main(int argc, char **argv) {
    unsigned char footer[ON1X_VM_FOOTER_SIZE];
    struct stat st;
    int self = open("/proc/self/exe", O_RDONLY | O_CLOEXEC);
    void *guest = NULL;
    int status = 2;
    if (self < 0 || fstat(self, &st) || !S_ISREG(st.st_mode) ||
        st.st_size <= (off_t)sizeof footer ||
        st.st_size > (off_t)(ON1X_VM_LAUNCHER_MAX + ON1X_VM_GUEST_MAX + sizeof footer) ||
        read_at(self, footer, sizeof footer, st.st_size - (off_t)sizeof footer) ||
        memcmp(footer, ON1X_VM_FOOTER_MAGIC, ON1X_VM_FOOTER_MAGIC_SIZE)) goto invalid;
    uint64_t size = 0;
    for (unsigned i = 0; i < 8; ++i)
        size |= (uint64_t)footer[ON1X_VM_FOOTER_LENGTH + i] << (i * 8);
    if (!size || size > ON1X_VM_GUEST_MAX ||
        size >= (uint64_t)(st.st_size - (off_t)sizeof footer)) goto invalid;
    switch (footer[ON1X_VM_FOOTER_POLICY]) {
    case ON1X_VM_ALWAYS:
        if (argc == 1) break;
        fputs("on1x VM launcher: guest arguments are not supported\n", stderr);
        goto done;
    case ON1X_VM_OPT_IN:
        if (argc == 2 && !strcmp(argv[1], "--sandbox")) break;
        fputs("on1x VM launcher: invoke with --sandbox; no native payload is present\n", stderr);
        goto done;
    default:
        goto invalid;
    }
    guest = malloc((size_t)size);
    if (!guest || read_at(self, guest, (size_t)size, st.st_size - (off_t)sizeof footer - (off_t)size)) {
        fputs("on1x VM launcher: could not read the embedded guest\n", stderr);
        goto done;
    }
    close(self);
    self = -1;
    status = on1x_archvirt_run_image(guest, (size_t)size);
    goto done;
invalid:
    fputs("on1x VM launcher: missing or invalid embedded guest\n", stderr);
done:
    if (self >= 0) close(self);
    free(guest);
    return status;
}
