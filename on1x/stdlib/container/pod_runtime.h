#ifndef ON1X_CONTAINER_POD_RUNTIME_H
#define ON1X_CONTAINER_POD_RUNTIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Private scalar ABI used by Pod.on1x. Successful results are bytes 0..255. */
enum {
    ON1X_POD_INVALID = -1,
    ON1X_POD_SYSTEM = -2,
    ON1X_POD_UNSUPPORTED = -3,
    ON1X_POD_DENIED = -4,
    ON1X_POD_LIMIT = -5,
    ON1X_POD_TIMEOUT = -6,
    ON1X_POD_PROTOCOL = -7,
    ON1X_POD_STATE = -9,
    ON1X_POD_NOT_AUTHORIZED = -11,
    ON1X_POD_CLEANUP = -12,
    ON1X_POD_WORKER_FAILED = -13
};

int64_t on1x_container_pod_echo_byte(int64_t byte, int64_t wall_ms,
                                    int64_t namespaces, int64_t cgroup);

/* Optional trusted native override. The default grants no cgroup authority.
 * Return a borrowed FD for an exclusively delegated cgroup v2 parent, or -1.
 * The hook runs on the single supervisor thread; it must not re-enter or block.
 * Never derive this FD from On1x, a notebook, or an untrusted launch environment.
 */
int on1x_container_pod_delegated_parent(void);

#ifdef __cplusplus
}
#endif

#endif
