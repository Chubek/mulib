#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#define MULIB_IMPL_SANDBOX_PROFILE
#define MULIB_IMPL_IPC_FRAMED
#define MULIB_IMPL_SHM
#define MULIB_IMPL_SECCOMP

#include "mulib-ipc-framed.h"
#include "mulib-seccomp.h"
#include "mulib-shm.h"
