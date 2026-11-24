#define _GNU_SOURCE
#include "runner.h"
#include "protocol.h"

#include <archvirt.h>
#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/signalfd.h>
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
    input_idle_seconds = 60,
    session_seconds = 300,
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
    return image;

io_failure:
    fputs("On1x ArchVirt: failed to copy or seal guest ELF\n", stderr);
    close(input);
    close(image);
    return -1;
}

enum live_reason {
    live_ok, live_active_timeout, live_idle_timeout, live_wall_timeout,
    live_input_error, live_output_error, live_watchdog_error, live_sigint, live_sigterm,
};

struct interactive {
    int input, output, error, cancelling, finished, signals;
    sigset_t old_signals;
    /* Monotonic milliseconds shifted left, with the low bit indicating a read wait. */
    atomic_llong deadline;
    atomic_int reason;
    long long wall;
    uint8_t parameter[ON1X_AV_INPUT_MAX];
    size_t cursor, size;
};

static long long now_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts)) return -1;
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void notify(int fd)
{
    const uint64_t one = 1;
    while (write(fd, &one, sizeof(one)) < 0 && errno == EINTR) {}
}

static void cancel(struct interactive *live, enum live_reason why)
{
    int expected = live_ok;
    if (atomic_compare_exchange_strong(&live->reason, &expected, why))
        notify(live->cancelling);
}

/* The KVM callback never waits on a host descriptor without also polling the
 * cancellation event. Cancellation is sticky, not consumed by the callback. */
static bool ready(struct interactive *live, int fd, short events, enum live_reason failure)
{
    struct pollfd p[2] = {{.fd = live->cancelling, .events = POLLIN}, {.fd = fd, .events = events}};
    int n;
    do { n = poll(p, 2, -1); } while (n < 0 && errno == EINTR);
    if (p[0].revents) return false;
    if (n > 0 && (p[1].revents & (events | POLLHUP))) return true;
    cancel(live, failure);
    return false;
}

static void set_activity(struct interactive *live, bool waiting)
{
    long long now = now_ms();
    if (now < 0) {
        cancel(live, live_watchdog_error);
        return;
    }
    atomic_store(&live->deadline,
                 2 * (now + 1000LL * (waiting ? input_idle_seconds : deadline_seconds)) + waiting);
}

static uint8_t collect_parameter(struct interactive *live)
{
    set_activity(live, true);
    for (size_t n = 0; n < sizeof(live->parameter); ++n) {
        uint8_t byte;
        ssize_t got;
        do {
            if (!ready(live, live->input, POLLIN, live_input_error)) return ON1X_AV_INPUT_EOF;
            got = read(live->input, &byte, 1);
        } while (got < 0 && (errno == EINTR || errno == EAGAIN));
        if (got < 0) {
            cancel(live, live_input_error);
            return ON1X_AV_INPUT_EOF;
        }
        if (!got) return n ? ON1X_AV_INPUT_INVALID : ON1X_AV_INPUT_EOF;
        if (byte != '\n' && !(byte >= '0' && byte <= '9') && !(n == 0 && (byte == '+' || byte == '-')))
            return ON1X_AV_INPUT_INVALID;
        live->parameter[n] = byte;
        if (byte == '\n') {
            live->cursor = 1;
            live->size = n + 1;
            return live->parameter[0];
        }
    }
    return ON1X_AV_INPUT_INVALID;
}

struct console {
    size_t length, stderr_at;
    enum { data, escape, exit_status, done, bad } phase;
    unsigned status;
    bool stderr_selected, input_requested;
    struct interactive *live;
    unsigned char bytes[max_console];
};

static uint8_t read_parameter(struct console *c)
{
    if (!c->input_requested) return ON1X_AV_INPUT_EOF;
    struct interactive *live = c->live;
    if (!live) {
        c->input_requested = false;
        return ON1X_AV_INPUT_UNAVAILABLE;
    }
    uint8_t result;
    if (live->cursor < live->size) {
        result = live->parameter[live->cursor++];
    } else {
        result = collect_parameter(live);
        set_activity(live, false);
    }
    if (live->cursor == live->size) {
        live->cursor = live->size = 0;
        c->input_requested = false;
    }
    return result;
}

static bool write_live(struct interactive *live, int fd, uint8_t byte)
{
    ssize_t n;
    if (atomic_load(&live->reason)) return false;
    do {
        n = write(fd, &byte, 1);
        if (n < 0 && errno == EAGAIN && !ready(live, fd, POLLOUT, live_output_error)) return false;
    } while (n < 0 && (errno == EAGAIN || errno == EINTR));
    if (n == 1) return true;
    cancel(live, live_output_error);
    return false;
}

static uint8_t serial(void *user, int is_write, uint8_t byte)
{
    struct console *c = user;
    if (!is_write) return read_parameter(c);
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
        } else if (byte == ON1X_AV_READ_I64 && !c->input_requested && !c->stderr_selected) {
            c->input_requested = true;
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
    if (c->length == max_console) {
        c->phase = bad;
        if (c->live) cancel(c->live, live_output_error);
    } else if (c->live) {
        if (write_live(c->live, c->stderr_selected ? c->live->error : c->live->output, byte))
            ++c->length;
    } else {
        c->bytes[c->length++] = byte;
    }
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

static int initialize(void *vm, const char *path, struct console *c)
{
    ArchVirtConfig config = {
        .struct_size = sizeof(config), .machine = "x86_64", .backend = "kvm",
        .kernel_path = path, .memory_mb = ON1X_AV_MEMORY_MB, .flags = ARCHVIRT_DOMIOS,
    };
    if (archvirt_set_serial(vm, serial, c) || archvirt_initialize(vm, &config)) {
        if (c->live)
            dprintf(c->live->error, "On1x ArchVirt: DomIOS/KVM initialization failed: %s\n", archvirt_last_error(vm));
        else
            fprintf(stderr, "On1x ArchVirt: DomIOS/KVM initialization failed: %s\n", archvirt_last_error(vm));
        return launch_failure;
    }
    return 0;
}

static int run(void *vm, const char *path, struct console *c)
{
    if (initialize(vm, path, c)) return launch_failure;
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

struct live_execution { struct interactive *live; void *vm; };

static void *watch_interactive(void *arg)
{
    struct live_execution *exec = arg;
    struct interactive *live = exec->live;
    struct pollfd p[] = {
        {.fd = live->finished, .events = POLLIN},
        {.fd = live->cancelling, .events = POLLIN},
        {.fd = live->signals, .events = POLLIN},
    };
    for (;;) {
        const long long now = now_ms();
        const long long activity = atomic_load(&live->deadline);
        const long long due = activity / 2;
        const long long next = due < live->wall ? due : live->wall;
        const int wait = next <= now ? 0 : next - now > 100 ? 100 : (int)(next - now);
        int n = poll(p, sizeof(p) / sizeof(p[0]), wait);
        if (p[0].revents & POLLIN) return NULL;
        if (p[1].revents & POLLIN) break;
        if (p[2].revents & POLLIN) {
            struct signalfd_siginfo signal;
            if (read(live->signals, &signal, sizeof(signal)) != (ssize_t)sizeof(signal)) {
                cancel(live, live_watchdog_error);
            } else {
                cancel(live, signal.ssi_signo == SIGINT ? live_sigint : live_sigterm);
            }
            break;
        }
        if (n < 0 && errno == EINTR) continue;
        if (n < 0 || now < 0 || p[0].revents || p[1].revents || p[2].revents) {
            cancel(live, live_watchdog_error);
            break;
        }
        if (now >= live->wall) {
            cancel(live, live_wall_timeout);
            break;
        }
        if (now >= due && atomic_load(&live->deadline) == activity) {
            cancel(live, (activity & 1) ? live_idle_timeout : live_active_timeout);
            break;
        }
    }
    archvirt_stop(exec->vm);
    return NULL;
}

static int run_interactive(void *vm, const char *path, struct console *c)
{
    struct interactive *live = c->live;
    if (initialize(vm, path, c)) return launch_failure;
    long long now = now_ms();
    if (now < 0) {
        dprintf(live->error, "On1x ArchVirt: cannot set interactive deadline\n");
        return launch_failure;
    }
    live->wall = now + 1000LL * session_seconds;
    atomic_store(&live->deadline, 2 * (now + 1000LL * deadline_seconds));
    if (archvirt_start(vm)) {
        dprintf(live->error, "On1x ArchVirt: cannot start guest: %s\n", archvirt_last_error(vm));
        return launch_failure;
    }
    struct live_execution exec = {.live = live, .vm = vm};
    pthread_t watcher;
    int error = pthread_create(&watcher, NULL, watch_interactive, &exec);
    if (error) {
        cancel(live, live_watchdog_error); /* Wake any callback before joining the VM. */
        archvirt_stop(vm);
        archvirt_wait(vm);
        dprintf(live->error, "On1x ArchVirt: cannot start interactive watchdog: %s\n", strerror(error));
        return launch_failure;
    }
    int waited = archvirt_wait(vm);
    notify(live->finished);
    pthread_join(watcher, NULL);
    const char *diagnostic = NULL;
    int status = launch_failure;
    switch (atomic_load(&live->reason)) {
    case live_sigint: return 130;
    case live_sigterm: return 143;
    case live_active_timeout: diagnostic = "interactive guest exceeded its 5-second active deadline"; break;
    case live_idle_timeout: diagnostic = "interactive guest exceeded its 60-second parameter idle deadline"; break;
    case live_wall_timeout: diagnostic = "interactive guest exceeded its 300-second wall deadline"; break;
    case live_input_error: diagnostic = "cannot read interactive parameter pipe"; break;
    case live_output_error: diagnostic = "interactive console is unavailable or over its 1 MiB limit"; break;
    case live_watchdog_error: diagnostic = "interactive watchdog failed"; break;
    case live_ok:
        if (waited)
            dprintf(live->error, "On1x ArchVirt: guest execution failed: %s\n", archvirt_last_error(vm));
        else if (c->phase != done)
            diagnostic = "guest halted without a valid terminal record (invalid/over-limit serial)";
        else
            status = (int)c->status;
        break;
    }
    if (diagnostic) dprintf(live->error, "On1x ArchVirt: %s\n", diagnostic);
    return status;
}

/* Reopen pipes/terminals independently: O_NONBLOCK on dup() would leak to
 * parent/shared open file descriptions. Exclude blocking regular files. */
static int live_stdio(int original, bool input)
{
    struct stat info;
    if (fstat(original, &info)) return -1;
    if (!S_ISFIFO(info.st_mode)) {
        if (input || !S_ISCHR(info.st_mode)) return -1;
        if (!isatty(original)) {
            struct stat null;
            if (stat("/dev/null", &null) || !S_ISCHR(null.st_mode) || info.st_rdev != null.st_rdev)
                return -1;
        }
    }
    char path[64];
    snprintf(path, sizeof(path), "/proc/self/fd/%d", original);
    return open(path, (input ? O_RDONLY : O_WRONLY) | O_NONBLOCK | O_CLOEXEC | O_NOCTTY);
}

static void close_interactive(struct interactive *live)
{
    const int fds[] = {live->input, live->output, live->error, live->cancelling,
                       live->finished, live->signals};
    for (size_t i = 0; i < sizeof(fds) / sizeof(fds[0]); ++i)
        if (fds[i] >= 0) close(fds[i]);
    if (!sigismember(&live->old_signals, SIGPIPE)) {
        /* Avoid restoring an unblocked SIGPIPE from failed diagnostic writes. */
        sigset_t pipe;
        sigemptyset(&pipe);
        sigaddset(&pipe, SIGPIPE);
        const struct timespec no_wait = {0};
        (void)sigtimedwait(&pipe, NULL, &no_wait);
    }
    (void)pthread_sigmask(SIG_SETMASK, &live->old_signals, NULL);
}

static bool open_interactive(struct interactive *live)
{
    sigset_t signals, blocked;
    sigemptyset(&signals);
    sigaddset(&signals, SIGINT);
    sigaddset(&signals, SIGTERM);
    blocked = signals;
    sigaddset(&blocked, SIGPIPE);
    if (pthread_sigmask(SIG_BLOCK, &blocked, &live->old_signals)) return false;
    live->input = live_stdio(STDIN_FILENO, true);
    live->output = live_stdio(STDOUT_FILENO, false);
    live->error = live_stdio(STDERR_FILENO, false);
    live->cancelling = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    live->finished = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    live->signals = signalfd(-1, &signals, SFD_CLOEXEC | SFD_NONBLOCK);
    if (live->input >= 0 && live->output >= 0 && live->error >= 0 &&
        live->cancelling >= 0 && live->finished >= 0 && live->signals >= 0)
        return true;
    close_interactive(live);
    return false;
}

int on1x_archvirt_check_sealed(int image)
{
    const int all = F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_SEAL;
    const int seals = fcntl(image, F_GET_SEALS);
    struct stat st;
    if (seals < 0 || (seals & all) != all || fstat(image, &st) ||
        !S_ISREG(st.st_mode) || st.st_size <= 0 || st.st_size > max_elf ||
        !is_guest(image, (uint64_t)st.st_size)) {
        fputs("On1x ArchVirt: input is not an On1x DomIOS x86-64 guest ELF\n", stderr);
        return launch_failure;
    }
    return 0;
}

static int run_sealed(int image, struct interactive *live)
{
    if (on1x_archvirt_check_sealed(image)) return launch_failure;
    char path[64];
    snprintf(path, sizeof(path), "/proc/self/fd/%d", image);
    struct console *c = calloc(1, sizeof(*c));
    void *vm = c ? archvirt_create() : NULL;
    int result = launch_failure;
    if (vm) {
        c->live = live;
        result = live ? run_interactive(vm, path, c) : run(vm, path, c);
        archvirt_destroy(vm);
    } else {
        fputs("On1x ArchVirt: cannot create guest runtime\n", stderr);
    }
    free(c);
    return result;
}

int on1x_archvirt_run_sealed(int image) { return run_sealed(image, NULL); }

static int from_path(const char *guest_elf, bool execute, struct interactive *live)
{
    if (!guest_elf || !*guest_elf) {
        fputs("On1x ArchVirt: a guest ELF path is required\n", stderr);
        return launch_failure;
    }
    int image = snapshot(guest_elf);
    if (image < 0) return launch_failure;
    const int result = execute ? run_sealed(image, live) : on1x_archvirt_check_sealed(image);
    close(image);
    return result;
}

int on1x_archvirt_run_path(const char *guest_elf) { return from_path(guest_elf, true, NULL); }
int on1x_archvirt_check_path(const char *guest_elf) { return from_path(guest_elf, false, NULL); }

int on1x_archvirt_run_interactive_path(const char *guest_elf)
{
    if (getuid() != geteuid() || getgid() != getegid()) {
        fputs("On1x ArchVirt: interactive runner unavailable in a set-ID process\n", stderr);
        return launch_failure;
    }
    struct interactive live = {0};
    if (!open_interactive(&live)) {
        fputs("On1x ArchVirt: interactive runner requires a stdin pipe and pipe/terminal console outputs\n",
              stderr);
        return launch_failure;
    }
    int result = from_path(guest_elf, true, &live);
    close_interactive(&live);
    return result;
}
