#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "pod_runtime.h"
#include "mulib-sandbox-process.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef F_SEAL_EXEC
#define F_SEAL_EXEC 0x0020
#endif

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s (errno=%d)\n", __FILE__, __LINE__, #condition, errno); \
        exit(1); \
    } \
} while (0)

static int delegated = -1;
static int expire, reject_namespace, verify_cgroup, saw_attach, saw_stop, fail_stop;
static long live_child;

int on1x_container_pod_delegated_parent(void)
{
    return delegated;
}

int __real_mulib_sandbox_process_start(mulib_sandbox_process_t *,
                                      const mulib_sandbox_launch_t *);
int __wrap_mulib_sandbox_process_start(mulib_sandbox_process_t *child,
                                      const mulib_sandbox_launch_t *launch)
{
    const int required = F_SEAL_WRITE | F_SEAL_GROW | F_SEAL_SHRINK | F_SEAL_EXEC | F_SEAL_SEAL;
    int seals = fcntl(launch->trusted_elf_fd, F_GET_SEALS);
    CHECK(seals >= 0 && (seals & required) == required);
    CHECK(!launch->group && (launch->profile.required & MULIB_SB_BASELINE) == MULIB_SB_BASELINE);
    CHECK(launch->profile.memory_bytes == 64 * MULIB_SANDBOX_MIB);
    CHECK(launch->profile.cpu_seconds == 1 && launch->profile.max_open_files == 4);
    CHECK(launch->profile.max_frame_bytes == 8 && launch->profile.wall_ms <= 5000);
    mulib_sandbox_launch_t config = *launch;
    if (reject_namespace) {
        CHECK(config.namespace_spec.flags);
        config.namespace_spec.inside_uid = UINT32_MAX;
    }
    int rc = __real_mulib_sandbox_process_start(child, &config);
    if (!rc)
        live_child = child->pid;
    return rc;
}

int __real_mulib_sandbox_process_receive(mulib_sandbox_process_t *, void *,
                                        size_t, size_t *);
int __real_mulib_sandbox_process_send(mulib_sandbox_process_t *, const void *, size_t);
int __wrap_mulib_sandbox_process_send(mulib_sandbox_process_t *child, const void *bytes,
                                     size_t size)
{
    /* Fault-inject a lost request. The real worker remains blocked awaiting it;
     * the real receiver must wait out its independent deadline. */
    return expire ? 0 : __real_mulib_sandbox_process_send(child, bytes, size);
}

int __wrap_mulib_sandbox_process_receive(mulib_sandbox_process_t *child, void *bytes,
                                        size_t capacity, size_t *size)
{
    if (expire)
        CHECK(!mulib_ipc_deadline(25, &child->deadline));
    return __real_mulib_sandbox_process_receive(child, bytes, capacity, size);
}

int __real_mulib_sandbox_process_stop(mulib_sandbox_process_t *, int64_t, mulib_sandbox_exit_t *);
int __wrap_mulib_sandbox_process_stop(mulib_sandbox_process_t *child, int64_t deadline,
                                     mulib_sandbox_exit_t *status)
{
    CHECK(!child->group);
    if (fail_stop) {
        fail_stop = 0;
        return MULIB_SB_TIMEOUT;
    }
    int rc = __real_mulib_sandbox_process_stop(child, deadline, status);
    if (expire && !rc) {
        CHECK(status->signal_number == SIGKILL);
        saw_stop = 1;
    }
    return rc;
}

int __real_mulib_cgroup2_attach(const mulib_cgroup2_t *, long);
int __wrap_mulib_cgroup2_attach(const mulib_cgroup2_t *group, long child)
{
    CHECK(verify_cgroup && child > 0 && child == live_child);
    siginfo_t state = {0};
    CHECK(!waitid(P_PID, (id_t)child, &state, WEXITED | WNOHANG | WNOWAIT));
    CHECK(!state.si_pid);
    int rc = __real_mulib_cgroup2_attach(group, child);
    saw_attach = !rc;
    return rc;
}

static const char *const ns_paths[] = {
    "/proc/self/ns/user", "/proc/self/ns/net", "/proc/self/ns/ipc", "/proc/self/ns/uts",
    "/proc/self/ns/mnt", "/proc/self/ns/pid"
};
static const uint32_t ns_flags[] = {
    MULIB_SB_NS_USER, MULIB_SB_NS_NETWORK, MULIB_SB_NS_IPC, MULIB_SB_NS_UTS,
    MULIB_SB_NS_MOUNT, MULIB_SB_NS_PID
};

static void ns_snapshot(struct stat out[6])
{
    for (size_t i = 0; i < 6; ++i)
        CHECK(!stat(ns_paths[i], &out[i]));
}

static void ns_verify(const struct stat a[6], const struct stat b[6], uint32_t flags)
{
    for (size_t i = 0; i < 6; ++i)
        CHECK(((a[i].st_ino != b[i].st_ino) || (a[i].st_dev != b[i].st_dev)) ==
              !!(flags & ns_flags[i]));
}

int __real_unshare(int);
int __wrap_unshare(int flags)
{
    struct stat before[6], after[6];
    uint32_t expected = 0;
    if (flags & CLONE_NEWUSER)
        expected |= MULIB_SB_NS_USER;
    if (flags & CLONE_NEWNET)
        expected |= MULIB_SB_NS_NETWORK;
    if (flags & CLONE_NEWIPC)
        expected |= MULIB_SB_NS_IPC;
    if (flags & CLONE_NEWUTS)
        expected |= MULIB_SB_NS_UTS;
    CHECK(expected && !(flags & ~(CLONE_NEWUSER | CLONE_NEWNET | CLONE_NEWIPC | CLONE_NEWUTS)));
    ns_snapshot(before);
    int rc = __real_unshare(flags);
    if (!rc) {
        ns_snapshot(after);
        ns_verify(before, after, expected);
    }
    return rc;
}

static int count_entries(DIR *directory)
{
    CHECK(directory);
    int count = 0;
    errno = 0;
    for (const struct dirent *entry; (entry = readdir(directory));)
        count += entry->d_name[0] != '.';
    CHECK(!errno);
    CHECK(!closedir(directory));
    return count;
}

static void no_children(void)
{
    int status;
    CHECK(waitpid(-1, &status, WNOHANG) == -1 && errno == ECHILD);
}

static void *concurrent(void *opaque)
{
    int fd = *(const int *)opaque;
    char ch;
    return read(fd, &ch, 1) == 1 ? NULL : opaque;
}

static void baseline(void)
{
    const int fds = count_entries(opendir("/proc/self/fd"));
    CHECK(on1x_container_pod_echo_byte(-1, 1000, 0, 0) == ON1X_POD_INVALID);
    CHECK(on1x_container_pod_echo_byte(256, 1000, 0, 0) == ON1X_POD_INVALID);
    CHECK(on1x_container_pod_echo_byte(2, 0, 0, 0) == ON1X_POD_INVALID);
    CHECK(on1x_container_pod_echo_byte(2, INT64_MAX, 0, 0) == ON1X_POD_INVALID);
    CHECK(on1x_container_pod_echo_byte(2, 1000, 3, 0) == ON1X_POD_INVALID);
    CHECK(on1x_container_pod_echo_byte(2, 1000, 0, 2) == ON1X_POD_INVALID);
    CHECK(on1x_container_pod_echo_byte(2, 1000, 0, 1) == ON1X_POD_NOT_AUTHORIZED);
    for (int i = 0; i < 12; ++i)
        CHECK(on1x_container_pod_echo_byte(i & 1 ? 255 : 0, 2000, 0, 0) == (i & 1 ? 255 : 0));
    no_children();

    expire = 1;
    CHECK(on1x_container_pod_echo_byte(37, 5000, 0, 0) == ON1X_POD_TIMEOUT && saw_stop);
    no_children();
    fail_stop = 1;
    CHECK(on1x_container_pod_echo_byte(37, 5000, 0, 0) == ON1X_POD_CLEANUP && !fail_stop);
    siginfo_t pending = {0};
    CHECK(!waitid(P_PID, (id_t)live_child, &pending, WEXITED | WNOHANG | WNOWAIT));
    expire = 0;
    CHECK(on1x_container_pod_echo_byte(37, 2000, 0, 0) == 37);
    no_children();
    reject_namespace = 1;
    CHECK(on1x_container_pod_echo_byte(37, 2000, 2, 0) == ON1X_POD_DENIED);
    no_children();
    reject_namespace = 0;

    struct sigaction old, ignore = {0};
    ignore.sa_handler = SIG_IGN;
    CHECK(!sigaction(SIGCHLD, &ignore, &old));
    CHECK(on1x_container_pod_echo_byte(37, 2000, 0, 0) == ON1X_POD_STATE);
    CHECK(!sigaction(SIGCHLD, &old, NULL));

    int pipefd[2];
    pthread_t thread;
    CHECK(!pipe(pipefd));
    CHECK(!pthread_create(&thread, NULL, concurrent, &pipefd[0]));
    CHECK(on1x_container_pod_echo_byte(37, 2000, 0, 0) == ON1X_POD_STATE);
    CHECK(write(pipefd[1], "x", 1) == 1);
    void *result;
    CHECK(!pthread_join(thread, &result) && !result);
    close(pipefd[0]);
    close(pipefd[1]);

    delegated = open(".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    CHECK(delegated >= 0);
    CHECK(on1x_container_pod_echo_byte(73, 2000, 0, 1) == ON1X_POD_INVALID);
    close(delegated);
    delegated = -1;
    no_children();
    CHECK(on1x_container_pod_echo_byte(73, 2000, 0, 0) == 73);
    no_children();
    CHECK(count_entries(opendir("/proc/self/fd")) == fds);
    puts("Pod: bounded relay, authorization, denied namespace, deadline and cleanup retry, reaper and thread rejection");
}

static int namespaces(void)
{
    struct stat before[6], after[6];
    ns_snapshot(before);
    int skipped = 0;
    for (int mode = 1; mode <= 2; ++mode) {
        /* An independent disposable child distinguishes denied unshare from
         * miscellaneous startup failures. Child checks only real kernel state. */
        pid_t check = fork();
        CHECK(check >= 0);
        if (!check) {
            mulib_namespace_spec_t config = {MULIB_SB_NS_USER, 0, 0};
            if (mode == 2)
                config.flags |= MULIB_SB_NS_NETWORK | MULIB_SB_NS_IPC | MULIB_SB_NS_UTS;
            uint32_t applied;
            int rc = mulib_namespace_enter(&config, &applied);
            if (rc && (errno == EPERM || errno == EACCES || errno == ENOSYS ||
                       errno == ENOSPC || errno == EINVAL))
                _exit(77);
            _exit(rc || applied != config.flags ? 1 : 0);
        }
        int status;
        CHECK(waitpid(check, &status, 0) == check && WIFEXITED(status));
        int64_t rc = on1x_container_pod_echo_byte(173, 2000, mode, 0);
        if (WEXITSTATUS(status) == 77) {
            CHECK(rc == ON1X_POD_DENIED);
            printf("SKIP namespace profile %d: host disallows unprivileged namespace setup\n", mode);
            skipped++;
        } else {
            CHECK(!WEXITSTATUS(status) && rc == 173);
            printf("Pod: child-only namespace profile %d verified against namespace identities\n", mode);
        }
        ns_snapshot(after);
        ns_verify(before, after, 0);
        no_children();
    }
    return skipped == 2 ? 77 : 0;
}

static void cgroups(const char *path)
{
    CHECK(path[0] == '/');
    delegated = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    CHECK(delegated >= 0);
    int before = count_entries(opendir(path));
    verify_cgroup = 1;
    CHECK(on1x_container_pod_echo_byte(22, 2000, 0, 1) == 22 && saw_attach);
    no_children();
    CHECK(count_entries(opendir(path)) == before);
    expire = 1;
    saw_attach = 0;
    CHECK(on1x_container_pod_echo_byte(22, 5000, 0, 1) == ON1X_POD_TIMEOUT && saw_attach && saw_stop);
    no_children();
    CHECK(count_entries(opendir(path)) == before);
    close(delegated);
    puts("Pod: delegated cgroup limits, attachment, normal/timeout teardown");
}

int main(int argc, char **argv)
{
    if (!getuid() || !getgid() || getuid() != geteuid() || getgid() != getegid()) {
        puts("SKIP Pod test requires a non-root, non-set-ID process");
        return 77;
    }
    if (argc == 2 && !strcmp(argv[1], "namespaces"))
        return namespaces();
    if (argc == 3 && !strcmp(argv[1], "cgroup")) {
        cgroups(argv[2]);
        return 0;
    }
    CHECK(argc == 1);
    baseline();
    return 0;
}
