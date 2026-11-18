#include "mulib.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* Check prototypes in a separate declaration-only TU. Do not invoke
 * enforcement, bootstrap or delegated filesystem APIs in this SDK test. */
_Static_assert(_Generic(&mulib_sandbox_process_start,
    int (*)(mulib_sandbox_process_t *, const mulib_sandbox_launch_t *): 1, default: 0), "launch ABI");
_Static_assert(_Generic(&mulib_sandbox_process_stop,
    int (*)(mulib_sandbox_process_t *, int64_t, mulib_sandbox_exit_t *): 1, default: 0), "stop ABI");
_Static_assert(_Generic(&mulib_landlock_restrict,
    int (*)(const mulib_landlock_rule_t *, size_t, unsigned): 1, default: 0), "rule slice ABI");
_Static_assert(_Generic(&mulib_namespace_enter,
    int (*)(const mulib_namespace_spec_t *, uint32_t *): 1, default: 0), "namespace ABI");
_Static_assert(_Generic(&mulib_cgroup2_create,
    int (*)(mulib_cgroup2_t *, int, const char *, const mulib_cgroup2_limits_t *): 1, default: 0), "cgroup ABI");
_Static_assert(_Generic(&mulib_seccomp_install,
    int (*)(const mulib_seccomp_policy_t *): 1, default: 0), "seccomp ABI");
_Static_assert(_Generic(((mulib_capbroker_op_t *)0)->handler,
    int (*)(void *, const char *, const void *, size_t, void *, size_t, size_t *): 1, default: 0), "callback ABI");

int main(void) {
    mulib_sandbox_profile_t p = mulib_sandbox_profile_default();
    if (mulib_sandbox_profile_validate(&p) != MULIB_SB_OK) return 1;
    p.version = 0;
    if (mulib_sandbox_profile_validate(&p) != MULIB_SB_INVALID) return 2;

    const char *asked[] = {"read", "write"};
    const char *allowed[] = {"read"};
    const char *effective[2] = {NULL, NULL};
    size_t count = 0;
    if (mulib_capbroker_intersect(asked, 2, allowed, 1, effective, 2, &count) != MULIB_SB_OK ||
        count != 1 || effective[0] != allowed[0] || effective[1]) return 3;
    count = 99;
    if (mulib_capbroker_intersect(asked, 2, allowed, 1, effective, 0, &count) == MULIB_SB_OK ||
        count != 99) return 4;

    int fd[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fd)) return 5;
    mulib_ipc_framed_t sender, receiver;
    int64_t deadline;
    const unsigned char data[] = {0, 1, 0xff};
    unsigned char received[8];
    int rc = 0;
    if (mulib_ipc_framed_init(&sender, fd[0], 1024) != MULIB_SB_OK ||
        mulib_ipc_framed_init(&receiver, fd[1], 1024) != MULIB_SB_OK ||
        mulib_ipc_deadline(1000, &deadline) != MULIB_SB_OK ||
        mulib_ipc_send(&sender, data, sizeof data, deadline) != MULIB_SB_OK ||
        mulib_ipc_receive(&receiver, received, sizeof received, &count, deadline) != MULIB_SB_OK ||
        count != sizeof data || memcmp(data, received, count)) rc = 6;
    if (close(fd[0]) || close(fd[1])) return 7;
    if (rc) return rc;
    /* Compare measured C layout with the SDK graph in the test runner. */
    return printf("%zu %zu %zu\n%zu %zu %zu\n%zu %zu %zu\n",
        sizeof(mulib_sandbox_profile_t), _Alignof(mulib_sandbox_profile_t),
        offsetof(mulib_sandbox_profile_t, max_broker_bytes) * CHAR_BIT,
        sizeof(mulib_sandbox_process_t), _Alignof(mulib_sandbox_process_t),
        offsetof(mulib_sandbox_process_t, group) * CHAR_BIT,
        sizeof(mulib_capbroker_op_t), _Alignof(mulib_capbroker_op_t),
        offsetof(mulib_capbroker_op_t, handler) * CHAR_BIT) < 0;
}
