/* Example: trusted bootstrap, bounded worker request, supervisor-side reap.
 * Compile from repository root:
 * cc -D_GNU_SOURCE -std=c11 -Wall -Wextra -Wpedantic -Werror -Ikernels \
 *     examples/modules/sandbox-process.c -o /tmp/mulib-worker-demo
 * /tmp/mulib-worker-demo
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define MULIB_IMPL_SANDBOX_PROFILE
#define MULIB_IMPL_SECCOMP
#define MULIB_IMPL_LANDLOCK
#define MULIB_IMPL_NAMESPACE
#define MULIB_IMPL_CGROUP2
#define MULIB_IMPL_IPC_FRAMED
#define MULIB_IMPL_SANDBOX_PROCESS
#include "mulib-sandbox-process.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

static int worker(void) {
    mulib_ipc_framed_t ipc;
    mulib_sandbox_profile_t profile;
    if (mulib_sandbox_worker_enter(&ipc, &profile)) return 125;
    int64_t deadline;
    char bytes[256];
    size_t n;
    if (mulib_ipc_deadline(profile.wall_ms, &deadline) ||
        mulib_ipc_receive(&ipc, bytes, sizeof bytes, &n, deadline)) return 2;
    /* The plugin's application code begins HERE. */
    for (size_t i = 0; i < n; ++i)
        if (bytes[i] >= 'a' && bytes[i] <= 'z') bytes[i] -= ('a' - 'A');
    return mulib_ipc_send(&ipc, bytes, n, deadline) ? 3 : 0;
}
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--mulib-worker")) return worker();
    int elf = open("/proc/self/exe", O_RDONLY | O_CLOEXEC);
    if (elf < 0) return 1;
    mulib_sandbox_launch_t config = {0};
    config.trusted_elf_fd = elf;
    config.profile = mulib_sandbox_profile_default();
    config.profile.wall_ms = 3000;
    /* Demonstration only: production supplies a dedicated administrator-assigned ID. */
    if (geteuid() == 0) {
        config.drop_identity = 1;
        config.uid = config.gid = 65534;
    }
    mulib_sandbox_process_t p = MULIB_SANDBOX_PROCESS_INIT;
    int rc = mulib_sandbox_process_start(&p, &config);
    close(elf);
    char reply[256]; size_t n = 0;
    const char request[] = "hello from mulib";
    if (!rc) rc = mulib_sandbox_process_send(&p, request, sizeof request - 1);
    if (!rc) rc = mulib_sandbox_process_receive(&p, reply, sizeof reply, &n);
    mulib_sandbox_exit_t status = {0};
    int64_t cleanup;
    if (!mulib_ipc_deadline(3000, &cleanup) && p.pid > 0) {
        if (!rc) {
            int r;
            do {
                r = mulib_sandbox_process_tick(&p, cleanup, &status);
                if (r == MULIB_SB_AGAIN) {
                    struct timespec pause = {0, 1000000};
                    nanosleep(&pause, NULL);
                }
            } while (r == MULIB_SB_AGAIN && mulib_ipc_now_ms() < cleanup);
            if (r || !status.exited || status.exit_code) rc = r ? r : MULIB_SB_SYSTEM;
        }
        if (p.pid > 0) {
            int r = mulib_sandbox_process_stop(&p, cleanup, &status);
            if (r) rc = r;
        }
    }
    if (rc) { fprintf(stderr, "sandbox: %s\n", mulib_sandbox_strerror(rc)); return 1; }
    printf("%.*s\n", (int)n, reply);
    return 0;
}
