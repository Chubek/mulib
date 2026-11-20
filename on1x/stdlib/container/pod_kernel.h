/* Used only for the fixed standalone worker image; the application instead
 * links the normal MuLIB implementation TU selected by the build plan. */
#ifndef ON1X_CONTAINER_POD_KERNEL_H
#define ON1X_CONTAINER_POD_KERNEL_H

#define MULIB_IMPL_SANDBOX_PROFILE
#define MULIB_IMPL_SECCOMP
#define MULIB_IMPL_LANDLOCK
#define MULIB_IMPL_NAMESPACE
#define MULIB_IMPL_CGROUP2
#define MULIB_IMPL_IPC_FRAMED
#define MULIB_IMPL_SANDBOX_PROCESS
#include "mulib-sandbox-process.h"

#endif
