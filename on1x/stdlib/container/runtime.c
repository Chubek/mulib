/* The planner enables just the needed leaf groups and links all selected
 * MuLIB implementations in its separate, single implementation TU. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#include "runtime.h"

#if !defined(__linux__)
#error "On1x container native bridge supports Linux only"
#endif

#ifdef ON1X_CONTAINER_TLS
#include "mulib-xthreads.h"

struct tls_value {
    int64_t value;
    bool present;
};

static mulib_xonce_t tls_once = MULIB_XONCE_INIT;
static mulib_xtss_t tls_key;
static bool tls_key_ready;
/* The key borrows private native TLS storage. No heap object or pointer must
 * be destroyed by the key at thread exit; native TLS storage ends with it. */
static _Thread_local struct tls_value tls_value_storage;

static void init_tls_key(void)
{
    tls_key_ready = mulib_xtss_create(&tls_key) == MULIB_XTHREAD_OK;
}

static bool prepare_tls(void)
{
    return mulib_xcall_once(&tls_once, init_tls_key) == MULIB_XTHREAD_OK && tls_key_ready;
}

static struct tls_value *current_tls(void)
{
    return prepare_tls() ? mulib_xtss_get(tls_key) : NULL;
}

bool on1x_container_tls_i64_present(void)
{
    const struct tls_value *slot = current_tls();
    return slot && slot->present;
}

int64_t on1x_container_tls_i64_value(void)
{
    const struct tls_value *slot = current_tls();
    return slot && slot->present ? slot->value : 0;
}

bool on1x_container_tls_i64_set(int64_t value)
{
    if (!prepare_tls())
        return false;
    struct tls_value *slot = mulib_xtss_get(tls_key);
    if (!slot) {
        if (mulib_xtss_set(tls_key, &tls_value_storage) != MULIB_XTHREAD_OK)
            return false;
        slot = &tls_value_storage;
    }
    *slot = (struct tls_value){value, true};
    return true;
}

void on1x_container_tls_i64_clear(void)
{
    struct tls_value *slot = current_tls();
    if (slot)
        *slot = (struct tls_value){0, false};
    /* Keep the empty binding so restoring a previously set value requires
     * no further setspecific call, even if the callback cleared the slot. */
}
#endif

#ifdef ON1X_CONTAINER_FOREIGN
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_PATH_BYTES 4095
_Static_assert(sizeof(off_t) >= sizeof(int64_t) && CHAR_BIT == 8,
               "container file bridge requires 64-bit file offsets and octets");

int64_t on1x_container_file_byte_at(const char *path, int64_t offset)
{
    if (!path || offset < 0 || offset == INT64_MAX || !*path ||
        strnlen(path, MAX_PATH_BYTES + 1) > MAX_PATH_BYTES)
        return -1;

    /* CLOEXEC is atomic with acquisition. NONBLOCK avoids waiting on FIFO
     * writers before fstat rejects them; this is not nonblocking file I/O. */
    int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK | O_NOCTTY);
    if (fd < 0)
        return -2;

    int64_t result = -4;
    struct stat info;
    if (fstat(fd, &info) == 0) {
        if (!S_ISREG(info.st_mode)) {
            result = -3;
        } else {
            unsigned char byte;
            ssize_t count = pread(fd, &byte, sizeof byte, (off_t)offset);
            if (count == 1)
                result = byte;
            else if (count == 0)
                result = 256;
        }
    }
    /* Linux releases the FD even on an interrupted close. Never retry and
     * risk closing another owner's reused descriptor. */
    if (close(fd) != 0)
        return -5;
    return result;
}
#endif
