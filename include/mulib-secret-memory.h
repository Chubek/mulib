/** @file mulib-secret-memory.h
 * @brief Owning secret buffer with guard pages, lock policy and full-capacity erase.
 * Define MULIB_IMPL_SECRET_MEMORY once. Dependencies need their own implementation switches.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_MEMORY_H
#define MULIB_SECRET_MEMORY_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-secret-core.h"
#ifdef __cplusplus
extern "C" {
#endif
enum {
    MULIB_SM_GUARDED = 1u,
    MULIB_SM_DONTDUMP = 2u,
    MULIB_SM_WIPEONFORK = 4u
};
typedef struct {
    unsigned char *ptr;
    size_t len, cap;
    unsigned locked, hardening;
    void *mapping;
    size_t mapping_size;
} mulib_sm_buffer;
/* Zero-init first; single owner; never copy or modify bookkeeping except len.
 * Allocation failure leaves b unchanged. On POSIX, cap is rounded up to a
 * page and the data pages have an inaccessible guard page on each side.
 * require_lock != 0 fails closed if pages cannot be locked. On platforms
 * without a POSIX implementation, require_lock == 0 allows heap storage.
 * Dump/fork advice is best effort where exposed by the OS headers (e.g.
 * Linux with _DEFAULT_SOURCE); hardening reports the successfully set flags.
 * These flags do not cover other copies, OS snapshots, or privileged reads.
 * Do not use an inherited buffer in a fork child; create a new session. */
int mulib_sm_alloc(mulib_sm_buffer *b, size_t cap, int require_lock);
/* Allows a source slice of b->ptr, but not of the ownership descriptor.
 * A successful assignment erases the unused tail; errors leave b and its
 * bytes unchanged. */
int mulib_sm_assign(mulib_sm_buffer *b, mulib_sc_view src);
void mulib_sm_clear(mulib_sm_buffer *b);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_MEMORY) && !defined(MULIB_SECRET_MEMORY_IMPLEMENTED)
#define MULIB_SECRET_MEMORY_IMPLEMENTED
#include <string.h>
#include <stdlib.h>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif
int mulib_sm_alloc(mulib_sm_buffer *b, size_t cap, int require_lock)
{
    if (!b || b->ptr || !cap)
        return MULIB_SC_INVALID;
    if (cap > 64u * 1024u * 1024u)
        return MULIB_SC_LIMIT;

    mulib_sm_buffer t;
    memset(&t, 0, sizeof t);
#if defined(__unix__) || defined(__APPLE__)
    long pg = sysconf(_SC_PAGESIZE);
    if (pg <= 0)
        return MULIB_SC_IO;
    size_t page = (size_t)pg;
    if (cap > SIZE_MAX - (page - 1))
        return MULIB_SC_LIMIT;
    t.cap = ((cap + page - 1) / page) * page;
    if (page > (SIZE_MAX - t.cap) / 2)
        return MULIB_SC_LIMIT;
    t.mapping_size = t.cap + 2 * page;

#if defined(MAP_ANONYMOUS)
    t.mapping = mmap(NULL, t.mapping_size, PROT_NONE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#elif defined(MAP_ANON)
    t.mapping = mmap(NULL, t.mapping_size, PROT_NONE,
                     MAP_PRIVATE | MAP_ANON, -1, 0);
#else
    /* Strict POSIX feature settings can hide anonymous mmap extensions. */
    int fd = open("/dev/zero", O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return MULIB_SC_IO;
    t.mapping = mmap(NULL, t.mapping_size, PROT_NONE, MAP_PRIVATE, fd, 0);
    (void)close(fd);
#endif
    if (t.mapping == MAP_FAILED)
        return MULIB_SC_NOMEM;
    t.ptr = (unsigned char *)t.mapping + page;
    if (mprotect(t.ptr, t.cap, PROT_READ | PROT_WRITE) != 0) {
        (void)munmap(t.mapping, t.mapping_size);
        return MULIB_SC_IO;
    }
    t.hardening = MULIB_SM_GUARDED;
#if defined(MADV_DONTDUMP)
    if (madvise(t.ptr, t.cap, MADV_DONTDUMP) == 0)
        t.hardening |= MULIB_SM_DONTDUMP;
#endif
#if defined(MADV_WIPEONFORK)
    if (madvise(t.ptr, t.cap, MADV_WIPEONFORK) == 0)
        t.hardening |= MULIB_SM_WIPEONFORK;
#endif
    t.locked = mlock(t.ptr, t.cap) == 0;
    if (require_lock && !t.locked) {
        (void)munmap(t.mapping, t.mapping_size);
        return MULIB_SC_DENIED;
    }
#else
    if (require_lock)
        return MULIB_SC_UNSUPPORTED;
    t.ptr = (unsigned char *)calloc(1, cap);
    if (!t.ptr)
        return MULIB_SC_NOMEM;
    t.cap = cap;
#endif
    *b = t;
    return MULIB_SC_OK;
}

int mulib_sm_assign(mulib_sm_buffer *b, mulib_sc_view s)
{
    if (!b || !b->ptr || !mulib_sc_view_ok(s) || s.len > b->cap ||
        mulib_sc_overlap(s.ptr, s.len, b, sizeof *b))
        return MULIB_SC_INVALID;
    if (s.len)
        memmove(b->ptr, s.ptr, s.len);
    mulib_sc_wipe(b->ptr + s.len, b->cap - s.len);
    b->len = s.len;
    return MULIB_SC_OK;
}

void mulib_sm_clear(mulib_sm_buffer *b)
{
    if (!b)
        return;
    if (b->ptr) {
        mulib_sc_wipe(b->ptr, b->cap);
#if defined(__unix__) || defined(__APPLE__)
        /* Unmapping removes the lock after the full-capacity wipe. */
        (void)munmap(b->mapping, b->mapping_size);
#else
        free(b->ptr);
#endif
    }
    memset(b, 0, sizeof *b);
}
#ifdef __cplusplus
}
#endif
#endif
