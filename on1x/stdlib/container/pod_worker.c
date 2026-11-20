/* Fixed bootstrap only. Compile as a separate static ELF, never into the
 * application. It accepts one binary byte and has no plugin/exec/import path. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "pod_kernel.h"

int main(void)
{
    mulib_ipc_framed_t ipc;
    mulib_sandbox_profile_t profile;
    if (mulib_sandbox_worker_enter(&ipc, &profile))
        return 125;

    int64_t deadline;
    unsigned char byte;
    size_t size;
    if (mulib_ipc_deadline(profile.wall_ms, &deadline) ||
        mulib_ipc_receive(&ipc, &byte, sizeof byte, &size, deadline) ||
        size != sizeof byte)
        return 126;

    return mulib_ipc_send(&ipc, &byte, sizeof byte, deadline) ? 127 : 0;
}
