#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "pod_runtime.h"

#if defined(__linux__) && defined(__LP64__) && \
    (defined(__x86_64__) || defined(__aarch64__)) && \
    defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

__attribute__((weak))
int on1x_container_pod_delegated_parent(void)
{
    return -1;
}

#ifdef ON1X_CONTAINER_POD_EMBEDDED
#include "mulib-sandbox-process.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/capability.h>
#include <linux/magic.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/vfs.h>
#include <time.h>
#include <unistd.h>

/* MFD_EXEC was added in Linux 6.3. Older kernels and kernels that prohibit
 * executable memfds fail closed; never fall back to a path or a temporary ELF. */
#ifndef MFD_EXEC
#define MFD_EXEC 0x0010U
#endif
#ifndef F_SEAL_EXEC
#define F_SEAL_EXEC 0x0020
#endif

extern const unsigned char on1x_container_pod_worker_elf[];
extern const size_t on1x_container_pod_worker_elf_size;

_Static_assert((int)MULIB_SB_INVALID == ON1X_POD_INVALID &&
               (int)MULIB_SB_SYSTEM == ON1X_POD_SYSTEM &&
               (int)MULIB_SB_UNSUPPORTED == ON1X_POD_UNSUPPORTED &&
               (int)MULIB_SB_DENIED == ON1X_POD_DENIED &&
               (int)MULIB_SB_LIMIT == ON1X_POD_LIMIT &&
               (int)MULIB_SB_TIMEOUT == ON1X_POD_TIMEOUT &&
               (int)MULIB_SB_PROTOCOL == ON1X_POD_PROTOCOL &&
               (int)MULIB_SB_STATE == ON1X_POD_STATE,
               "On1x Pod status ABI disagrees with MuLIB");

/* Native-only single owner. Ordinarily empty on return; if the kernel fails
 * to finish cleanup by the deadline, retain the resource here and attempt
 * cleanup on the next call before admitting any new work. */
static struct {
    mulib_sandbox_process_t child;
    mulib_cgroup2_t group;
    int active;
} pod_scope = {MULIB_SANDBOX_PROCESS_INIT, MULIB_CGROUP2_INIT, 0};

static int pod_host_check(void)
{
    if (!getuid() || !getgid() || getuid() != geteuid() || getgid() != getegid())
        return ON1X_POD_DENIED;
    struct __user_cap_header_struct header = {_LINUX_CAPABILITY_VERSION_3, 0};
    struct __user_cap_data_struct caps[2] = {{0}};
    if (syscall(SYS_capget, &header, caps))
        return ON1X_POD_UNSUPPORTED;
    /* Ambient is a subset of permitted. Inheritable alone grants no authority:
     * the sealed ELF has no file capabilities and MuLIB clears the worker's
     * inheritable set before it accepts an application frame. */
    for (size_t i = 0; i < sizeof caps / sizeof caps[0]; ++i)
        if (caps[i].effective || caps[i].permitted)
            return ON1X_POD_DENIED;

    /* Defense in depth; native hosts must still guarantee exclusive reaping,
     * no concurrent thread creation, and no reentry from a signal handler. */
    DIR *tasks = opendir("/proc/self/task");
    if (!tasks)
        return ON1X_POD_UNSUPPORTED;
    struct statfs fs;
    int rc = 0, threads = 0;
    if (fstatfs(dirfd(tasks), &fs) || fs.f_type != PROC_SUPER_MAGIC) {
        rc = ON1X_POD_UNSUPPORTED;
    } else {
        errno = 0;
        for (const struct dirent *ent; (ent = readdir(tasks));) {
            if (ent->d_name[0] != '.' && ++threads > 1)
                break;
        }
        if (errno)
            rc = ON1X_POD_SYSTEM;
        else if (threads != 1)
            rc = ON1X_POD_STATE;
    }
    closedir(tasks);
    return rc;
}

static int pod_open_worker(int *out)
{
    size_t size = on1x_container_pod_worker_elf_size;
    if (size < 4 || size > 4 * MULIB_SANDBOX_MIB)
        return ON1X_POD_LIMIT;
    if (memcmp(on1x_container_pod_worker_elf, "\177ELF", 4))
        return ON1X_POD_INVALID;
    int fd = memfd_create("on1x-pod-worker", MFD_CLOEXEC | MFD_ALLOW_SEALING | MFD_EXEC);
    if (fd < 0) {
        if (errno == EINVAL || errno == ENOSYS)
            return ON1X_POD_UNSUPPORTED;
        if (errno == EPERM || errno == EACCES)
            return ON1X_POD_DENIED;
        return ON1X_POD_SYSTEM;
    }
    for (size_t offset = 0; offset < size;) {
        ssize_t n = write(fd, on1x_container_pod_worker_elf + offset, size - offset);
        if (n <= 0)
            goto fail;
        offset += (size_t)n;
    }
    if (fchmod(fd, S_IRUSR | S_IXUSR))
        goto fail;
    int seals = F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_EXEC | F_SEAL_SEAL;
    if (fcntl(fd, F_ADD_SEALS, seals) < 0)
        goto fail;
    *out = fd;
    return 0;
fail:
    close(fd);
    return ON1X_POD_SYSTEM;
}

static int pod_open_group(int parent, const mulib_cgroup2_limits_t *limits)
{
    /* Make collisions negligible without trusting any name from On1x or using
     * a stale caller-owned leaf. The existing create primitive is exclusive. */
    uint64_t nonce;
    if (getrandom(&nonce, sizeof nonce, GRND_NONBLOCK) != sizeof nonce)
        return ON1X_POD_SYSTEM;
    char name[64];
    int n = snprintf(name, sizeof name, "on1x-pod-%016llx",
                     (unsigned long long)nonce);
    if (n <= 0 || (size_t)n >= sizeof name)
        return ON1X_POD_SYSTEM;
    return mulib_cgroup2_create(&pod_scope.group, parent, name, limits);
}

static int pod_attach_group(const mulib_cgroup2_limits_t *limits)
{
    const mulib_cgroup2_t *group = &pod_scope.group;
    long pid = pod_scope.child.pid;
    int populated, rc = mulib_cgroup2_populated(group, &populated);
    if (rc || populated)
        return rc ? rc : ON1X_POD_STATE;
    rc = mulib_cgroup2_attach(group, pid);
    if (rc)
        return rc;
    rc = mulib_cgroup2_verify(group, limits);
    if (rc)
        return rc;

    char actual[64], expected[64];
    int n = snprintf(expected, sizeof expected, "%ld\n", pid);
    if (n <= 0 || (size_t)n >= sizeof expected)
        return ON1X_POD_SYSTEM;
    int fd = openat(group->fd, "cgroup.procs", O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0)
        return ON1X_POD_SYSTEM;
    size_t used = 0;
    rc = ON1X_POD_DENIED;
    /* Require EOF immediately after this worker's sole PID. No PID from
     * cgroup.procs is ever used for signaling or migration. */
    while (used < sizeof actual) {
        ssize_t got = read(fd, actual + used, sizeof actual - used);
        if (got < 0) {
            rc = ON1X_POD_SYSTEM;
            break;
        }
        if (!got) {
            if (used == (size_t)n && !memcmp(actual, expected, used))
                rc = 0;
            break;
        }
        used += (size_t)got;
    }
    close(fd);
    return rc;
}

static int pod_reap(void)
{
    for (;;) {
        int64_t now = mulib_ipc_now_ms();
        if (now < 0)
            return ON1X_POD_SYSTEM;
        if (now >= pod_scope.child.deadline)
            return ON1X_POD_TIMEOUT;
        mulib_sandbox_exit_t status = {0};
        int rc = mulib_sandbox_process_poll(&pod_scope.child, &status);
        if (rc != MULIB_SB_AGAIN)
            return rc ? rc : (!status.exited || status.exit_code ? ON1X_POD_WORKER_FAILED : 0);
        const struct timespec pause = {0, 1000000L};
        nanosleep(&pause, NULL);
    }
}

static int pod_cleanup(void)
{
    if (pod_scope.child.pid > 0) {
        int64_t deadline;
        mulib_sandbox_exit_t status = {0};
        if (mulib_ipc_deadline(2000, &deadline) ||
            mulib_sandbox_process_stop(&pod_scope.child, deadline, &status))
            return ON1X_POD_CLEANUP;
    }
    if (mulib_cgroup2_destroy(&pod_scope.group))
        return ON1X_POD_CLEANUP;
    return 0;
}

static int pod_echo(unsigned char byte, uint64_t wall_ms, uint32_t namespaces, int cgroup)
{
    int rc = pod_cleanup();
    if (rc)
        return rc;

    int parent = cgroup ? on1x_container_pod_delegated_parent() : -1;
    if (cgroup && parent < 0)
        return ON1X_POD_NOT_AUTHORIZED;
    int elf;
    rc = pod_open_worker(&elf);
    if (rc)
        return rc;

    const mulib_cgroup2_limits_t limits = {64 * MULIB_SANDBOX_MIB, 25000, 100000, 1, 1};
    if (cgroup)
        rc = pod_open_group(parent, &limits);
    mulib_sandbox_launch_t launch = {0};
    launch.trusted_elf_fd = elf;
    launch.profile = mulib_sandbox_profile_default();
    launch.profile.memory_bytes = 64 * MULIB_SANDBOX_MIB;
    launch.profile.cpu_seconds = 1;
    launch.profile.startup_ms = 2000;
    launch.profile.wall_ms = wall_ms;
    launch.profile.max_frame_bytes = 8; /* includes startup handshake size */
    launch.profile.max_broker_requests = 1;
    launch.profile.max_broker_bytes = 8;
    if (namespaces) {
        launch.namespace_spec.flags = namespaces;
        launch.profile.namespace_flags = namespaces;
        launch.profile.required |= MULIB_SB_ENFORCE_NAMESPACE;
    }
    /* The worker has to wait for our first application frame. Use the launcher
     * without a group borrow; attach and verify our worker before sending that
     * frame, so neither startup-failure cleanup nor stop invokes cgroup.kill. */
    if (!rc) {
        rc = mulib_sandbox_process_start(&pod_scope.child, &launch);
        if (rc == MULIB_SB_EOF)
            rc = ON1X_POD_DENIED;
    }
    close(elf);
    if (!rc && cgroup)
        rc = pod_attach_group(&limits);
    if (!rc)
        rc = mulib_sandbox_process_send(&pod_scope.child, &byte, sizeof byte);
    unsigned char echo = 0;
    size_t size = 0;
    if (!rc) {
        rc = mulib_sandbox_process_receive(&pod_scope.child, &echo, sizeof echo, &size);
        if (rc == MULIB_SB_EOF)
            rc = ON1X_POD_WORKER_FAILED;
        if (!rc && (size != sizeof echo || echo != byte))
            rc = ON1X_POD_PROTOCOL;
    }
    if (!rc)
        rc = pod_reap();
    int cleanup = pod_cleanup();
    if (cleanup)
        return cleanup;
    if (!rc)
        return echo;
    if (rc > 0 || rc == MULIB_SB_NOMEM || rc == MULIB_SB_EOF)
        return ON1X_POD_SYSTEM;
    return rc;
}
#endif

int64_t on1x_container_pod_echo_byte(int64_t byte, int64_t wall_ms,
                                    int64_t namespaces, int64_t cgroup)
{
    if (byte < 0 || byte > 255 || wall_ms < 1 || wall_ms > 5000 ||
        namespaces < 0 || namespaces > 2 || (cgroup != 0 && cgroup != 1))
        return ON1X_POD_INVALID;
#ifdef ON1X_CONTAINER_POD_EMBEDDED
    int rc = pod_host_check();
    if (rc)
        return rc;
    if (pod_scope.active)
        return ON1X_POD_STATE;
    pod_scope.active = 1;
    uint32_t flags = 0;
    if (namespaces >= 1)
        flags |= MULIB_SB_NS_USER;
    if (namespaces == 2)
        flags |= MULIB_SB_NS_NETWORK | MULIB_SB_NS_IPC | MULIB_SB_NS_UTS;
    rc = pod_echo((unsigned char)byte, (uint64_t)wall_ms, flags, (int)cgroup);
    pod_scope.active = 0;
    return rc;
#else
    return ON1X_POD_NOT_AUTHORIZED;
#endif
}
#else
int on1x_container_pod_delegated_parent(void)
{
    return -1;
}

int64_t on1x_container_pod_echo_byte(int64_t byte, int64_t wall_ms,
                                    int64_t namespaces, int64_t cgroup)
{
    (void)byte;
    (void)wall_ms;
    (void)namespaces;
    (void)cgroup;
    return ON1X_POD_UNSUPPORTED;
}
#endif
