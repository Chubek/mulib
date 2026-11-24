#define _GNU_SOURCE
#include "runner.h"
#include "protocol.h"

#include <archvirt.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#if !defined(__linux__) || !defined(__x86_64__)
#error "On1x ArchVirt runner requires native x86-64 Linux"
#endif

enum {
    launch_failure = 125,
    max_elf = 8 * 1024 * 1024,
    max_console = 1024 * 1024,
    deadline_seconds = 5,
};

/* Inspect immutable bytes, before handing the same sealed file to ArchVirt's
 * own ELF loader. This is a format/profile gate, not a trust assessment. */
static bool is_guest(int fd, uint64_t size)
{
    Elf64_Ehdr h;
    Elf64_Phdr ph[2];
    if (pread(fd, &h, sizeof(h), 0) != (ssize_t)sizeof(h) ||
        memcmp(h.e_ident, ELFMAG, SELFMAG) ||
        h.e_ident[EI_CLASS] != ELFCLASS64 || h.e_ident[EI_DATA] != ELFDATA2LSB ||
        h.e_ident[EI_VERSION] != EV_CURRENT || h.e_ident[EI_OSABI] != ELFOSABI_NONE ||
        h.e_ident[EI_ABIVERSION] ||
        h.e_version != EV_CURRENT || h.e_type != ET_EXEC || h.e_machine != EM_X86_64 ||
        h.e_flags || h.e_entry != ON1X_AV_KERNEL_BASE || h.e_ehsize != sizeof(h) ||
        h.e_phoff != sizeof(h) || h.e_phentsize != sizeof(ph[0]) ||
        h.e_phnum < 1 || h.e_phnum > 2)
        return false;
    size_t bytes = h.e_phnum * sizeof(ph[0]);
    if (pread(fd, ph, bytes, (off_t)h.e_phoff) != (ssize_t)bytes) return false;
    uint64_t end = ON1X_AV_KERNEL_BASE;
    for (size_t i = 0; i < h.e_phnum; ++i) {
        const Elf64_Phdr *p = &ph[i];
        if (p->p_type != PT_LOAD || p->p_paddr != p->p_vaddr ||
            p->p_vaddr < end || p->p_vaddr >= ON1X_AV_KERNEL_LIMIT ||
            !p->p_memsz || p->p_memsz > ON1X_AV_KERNEL_LIMIT - p->p_vaddr ||
            p->p_filesz > p->p_memsz || p->p_offset > size ||
            p->p_filesz > size - p->p_offset || p->p_align != 4096 ||
            (p->p_offset % p->p_align) != (p->p_vaddr % p->p_align) ||
            p->p_flags != (i == 0 ? PF_R | PF_X : PF_R | PF_W) ||
            (i == 0 && (p->p_vaddr != h.e_entry || p->p_offset != 4096 || !p->p_filesz)))
            return false;
        end = p->p_vaddr + p->p_memsz;
    }
    return true;
}

static int snapshot(const char *path)
{
    int input = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (input < 0) {
        fprintf(stderr, "On1x ArchVirt: cannot open guest: %s\n", strerror(errno));
        return -1;
    }
    struct stat st;
    if (fstat(input, &st) || !S_ISREG(st.st_mode) || st.st_size <= 0 || st.st_size > max_elf) {
        fputs("On1x ArchVirt: guest must be a nonempty regular ELF file up to 8 MiB\n", stderr);
        close(input);
        return -1;
    }
    int image = memfd_create("on1x-archvirt-guest", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (image < 0) {
        fprintf(stderr, "On1x ArchVirt: cannot stage guest: %s\n", strerror(errno));
        close(input);
        return -1;
    }
    unsigned char block[16384];
    off_t remaining = st.st_size;
    while (remaining > 0) {
        size_t amount = remaining > (off_t)sizeof(block) ? sizeof(block) : (size_t)remaining;
        ssize_t count = read(input, block, amount);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) goto io_failure;
        for (ssize_t written = 0; written < count;) {
            ssize_t step = write(image, block + written, (size_t)(count - written));
            if (step < 0 && errno == EINTR) continue;
            if (step <= 0) goto io_failure;
            written += step;
        }
        remaining -= count;
    }
    if (fchmod(image, S_IRUSR) ||
        fcntl(image, F_ADD_SEALS, F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL) < 0)
        goto io_failure;
    close(input);
    if (!is_guest(image, (uint64_t)st.st_size)) {
        fputs("On1x ArchVirt: input is not an On1x DomIOS x86-64 guest ELF\n", stderr);
        close(image);
        return -1;
    }
    return image;

io_failure:
    fputs("On1x ArchVirt: failed to copy or seal guest ELF\n", stderr);
    close(input);
    close(image);
    return -1;
}

struct console {
    size_t length, stderr_at;
    enum { data, escape, exit_status, done, bad } phase;
    unsigned status;
    bool stderr_selected;
    unsigned char bytes[max_console];
};

static uint8_t serial(void *user, int is_write, uint8_t byte)
{
    struct console *c = user;
    if (!is_write) return 0; /* Never poll or read host input. */
    switch (c->phase) {
    case data:
        if (byte == ON1X_AV_ESCAPE) { c->phase = escape; return 0; }
        break;
    case escape:
        c->phase = data;
        if (byte == ON1X_AV_ESCAPE) break;
        if (byte == ON1X_AV_STDERR && !c->stderr_selected) {
            c->stderr_selected = true;
            c->stderr_at = c->length;
        } else if (byte == ON1X_AV_EXIT) {
            c->phase = exit_status;
        } else {
            c->phase = bad;
        }
        return 0;
    case exit_status:
        c->status = byte;
        c->phase = done;
        return 0;
    case done:
    case bad:
        c->phase = bad;
        return 0;
    }
    if (c->length == max_console) c->phase = bad;
    else c->bytes[c->length++] = byte;
    return 0;
}

static bool write_console(const struct console *c)
{
    size_t out = c->stderr_selected ? c->stderr_at : c->length;
    size_t err = c->length - out;
    return (!out || (fwrite(c->bytes, 1, out, stdout) == out && !fflush(stdout))) &&
           (!err || (fwrite(c->bytes + out, 1, err, stderr) == err && !fflush(stderr)));
}

struct deadline {
    pthread_mutex_t lock;
    pthread_cond_t wake;
    struct timespec at;
    void *vm;
    bool finished;
    int wait_error;
};

static void *watchdog(void *opaque)
{
    struct deadline *d = opaque;
    pthread_mutex_lock(&d->lock);
    while (!d->finished && !d->wait_error)
        d->wait_error = pthread_cond_timedwait(&d->wake, &d->lock, &d->at);
    bool stop = !d->finished;
    pthread_mutex_unlock(&d->lock);
    if (stop) archvirt_stop(d->vm);
    return NULL;
}

static int run(void *vm, const char *path, struct console *c)
{
    ArchVirtConfig config = {
        .struct_size = sizeof(config), .machine = "x86_64", .backend = "kvm",
        .kernel_path = path, .memory_mb = ON1X_AV_MEMORY_MB, .flags = ARCHVIRT_DOMIOS,
    };
    if (archvirt_set_serial(vm, serial, c) || archvirt_initialize(vm, &config)) {
        fprintf(stderr, "On1x ArchVirt: DomIOS/KVM initialization failed: %s\n", archvirt_last_error(vm));
        return launch_failure;
    }
    struct deadline d = { .lock = PTHREAD_MUTEX_INITIALIZER, .vm = vm };
    pthread_condattr_t attr;
    int error = pthread_condattr_init(&attr);
    if (!error) {
        error = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
        if (!error) error = pthread_cond_init(&d.wake, &attr);
        pthread_condattr_destroy(&attr);
    }
    if (error) {
        fprintf(stderr, "On1x ArchVirt: cannot initialize watchdog: %s\n", strerror(error));
        pthread_mutex_destroy(&d.lock);
        return launch_failure;
    }
    int result = launch_failure;
    if (clock_gettime(CLOCK_MONOTONIC, &d.at)) {
        fprintf(stderr, "On1x ArchVirt: cannot set watchdog deadline: %s\n", strerror(errno));
        goto cleanup;
    }
    d.at.tv_sec += deadline_seconds;
    if (archvirt_start(vm)) {
        fprintf(stderr, "On1x ArchVirt: cannot start guest: %s\n", archvirt_last_error(vm));
        goto cleanup;
    }
    pthread_t thread;
    error = pthread_create(&thread, NULL, watchdog, &d);
    if (error) {
        archvirt_stop(vm);
        archvirt_wait(vm);
        fprintf(stderr, "On1x ArchVirt: cannot start watchdog: %s\n", strerror(error));
        goto cleanup;
    }
    int waited = archvirt_wait(vm);
    pthread_mutex_lock(&d.lock);
    d.finished = true;
    pthread_cond_signal(&d.wake);
    pthread_mutex_unlock(&d.lock);
    pthread_join(thread, NULL);
    if (!write_console(c)) {
        fputs("On1x ArchVirt: failed to forward guest console output\n", stderr);
    } else if (d.wait_error == ETIMEDOUT) {
        fputs("On1x ArchVirt: guest exceeded the 5-second KVM deadline\n", stderr);
    } else if (d.wait_error) {
        fprintf(stderr, "On1x ArchVirt: watchdog failed: %s\n", strerror(d.wait_error));
    } else if (waited) {
        fprintf(stderr, "On1x ArchVirt: guest execution failed: %s\n", archvirt_last_error(vm));
    } else if (c->phase != done) {
        fputs("On1x ArchVirt: guest halted without a valid terminal record (invalid/over-limit serial)\n", stderr);
    } else {
        result = (int)c->status;
    }
cleanup:
    pthread_cond_destroy(&d.wake);
    pthread_mutex_destroy(&d.lock);
    return result;
}

int on1x_archvirt_run_path(const char *guest_elf)
{
    if (!guest_elf || !*guest_elf) {
        fputs("On1x ArchVirt: a guest ELF path is required\n", stderr);
        return launch_failure;
    }
    int image = snapshot(guest_elf);
    if (image < 0) return launch_failure;
    char path[64];
    snprintf(path, sizeof(path), "/proc/self/fd/%d", image);
    struct console *c = calloc(1, sizeof(*c));
    void *vm = c ? archvirt_create() : NULL;
    int result = launch_failure;
    if (vm) {
        result = run(vm, path, c);
        archvirt_destroy(vm);
    } else {
        fputs("On1x ArchVirt: cannot create guest runtime\n", stderr);
    }
    free(c);
    close(image);
    return result;
}
