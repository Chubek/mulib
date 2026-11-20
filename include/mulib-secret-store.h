/** @file mulib-secret-store.h
 * @brief Durable encrypted blob storage using a trusted private POSIX directory descriptor.
 * Define MULIB_IMPL_SECRET_STORE once. Dependencies need their own implementation switches.
 * Development kernel; see manual/secret-kernels.md for security limits.
 */
#ifndef MULIB_SECRET_STORE_H
#define MULIB_SECRET_STORE_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-secret-crypto.h"
#ifdef __cplusplus
extern "C" {
#endif
/* dirfd is an already-open private directory owned by the effective user.
 * Caller verifies directory provenance (ancestry, ACLs, mount/filesystem)
 * and serializes all readers, writers and recovery with an interprocess lock.
 * Use a local filesystem with reliable atomic rename and file/directory fsync.
 * Only ciphertext, at most 32 MiB per blob. This is not a multi-blob
 * transaction, a rollback defense, or a secure deletion API.
 *
 * Build with _POSIX_C_SOURCE=200809L on POSIX. UNCERTAIN means a namespace
 * mutation may have committed (e.g. rename/unlink succeeded but directory
 * fsync failed). Keep the lock; reconcile durable state before another
 * mutation. Interrupted writes may leave .tmp-* files; no automatic cleanup
 * or promotion of these files is performed. Recovery is a caller duty.
 *
 * Read outputs may not overlap each other or name. On error written is
 * unchanged, and only bytes already read into out are erased. */
int mulib_sstore_read(int dirfd, const char *name, mulib_sc_buf out, size_t *written);
int mulib_sstore_write(const mulib_scr_provider *p, int dirfd, const char *name,
                        mulib_sc_view wire);
int mulib_sstore_remove(int dirfd, const char *name);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SECRET_STORE) && !defined(MULIB_SECRET_STORE_IMPLEMENTED)
#define MULIB_SECRET_STORE_IMPLEMENTED
#if defined(__unix__) || defined(__APPLE__)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif
#if defined(__unix__) || defined(__APPLE__)
#define MULIB_SSTORE_BLOB_MAX_ (32u * 1024u * 1024u)

static int mulib_sstore_dir_(int d)
{
    struct stat st;
    return fstat(d, &st) == 0 && S_ISDIR(st.st_mode) && st.st_uid == geteuid() &&
           !(st.st_mode & 0077);
}

static int mulib_sstore_name_(const char *n)
{
    return mulib_sc_name(n, 127) && n[0] != '.';
}

static int mulib_sstore_private_(const struct stat *st)
{
    return S_ISREG(st->st_mode) && st->st_uid == geteuid() &&
           !(st->st_mode & 07077) && st->st_nlink == 1 && st->st_size >= 0;
}

static int mulib_sstore_errno_(int err)
{
    if (err == ENOENT)
        return MULIB_SC_NOTFOUND;
    if (err == ELOOP || err == EACCES || err == EPERM)
        return MULIB_SC_DENIED;
    return MULIB_SC_IO;
}

/* Under the caller's lock, refuse to replace/remove non-private targets.
 * This precheck is not protection from an untrusted writer with dir access. */
static int mulib_sstore_target_(int d, const char *n, struct stat *st)
{
    if (fstatat(d, n, st, AT_SYMLINK_NOFOLLOW) != 0)
        return mulib_sstore_errno_(errno);
    return mulib_sstore_private_(st) ? MULIB_SC_OK : MULIB_SC_DENIED;
}

static int mulib_sstore_sync_(int fd)
{
    int r;
    do {
        r = fsync(fd);
    } while (r != 0 && errno == EINTR);
    return r;
}

int mulib_sstore_read(int d, const char *n, mulib_sc_buf o, size_t *w)
{
    if (!mulib_sstore_dir_(d) || !mulib_sstore_name_(n) || (!o.ptr && o.cap) || !w)
        return MULIB_SC_INVALID;
    size_t name_size = strlen(n) + 1;
    if (mulib_sc_overlap(o.ptr, o.cap, w, sizeof *w) ||
        mulib_sc_overlap(o.ptr, o.cap, n, name_size) ||
        mulib_sc_overlap(w, sizeof *w, n, name_size))
        return MULIB_SC_INVALID;

    struct stat before, st;
    int r = mulib_sstore_target_(d, n, &before);
    if (r != MULIB_SC_OK)
        return r;
    int fd = openat(d, n, O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    if (fd < 0)
        return mulib_sstore_errno_(errno);

    size_t pos = 0, total = 0;
    if (fstat(fd, &st) != 0)
        r = MULIB_SC_IO;
    else if (!mulib_sstore_private_(&st) || st.st_dev != before.st_dev || st.st_ino != before.st_ino)
        r = MULIB_SC_DENIED;
    else if ((uintmax_t)st.st_size > o.cap || (uintmax_t)st.st_size > MULIB_SSTORE_BLOB_MAX_)
        r = MULIB_SC_LIMIT;
    else
        total = (size_t)st.st_size;

    while (r == MULIB_SC_OK && pos < total) {
        ssize_t z = read(fd, o.ptr + pos, total - pos);
        if (z < 0 && errno == EINTR)
            continue;
        if (z <= 0) {
            r = MULIB_SC_IO;
            break;
        }
        pos += (size_t)z;
    }
    if (r == MULIB_SC_OK) {
        unsigned char extra;
        ssize_t z;
        do {
            z = read(fd, &extra, 1);
        } while (z < 0 && errno == EINTR);
        if (z != 0)
            r = MULIB_SC_IO;
    }
    if (close(fd) != 0 && r == MULIB_SC_OK)
        r = MULIB_SC_IO;
    if (r != MULIB_SC_OK) {
        mulib_sc_wipe(o.ptr, pos);
        return r;
    }
    *w = total;
    return MULIB_SC_OK;
}

int mulib_sstore_write(const mulib_scr_provider *p, int d, const char *n, mulib_sc_view w)
{
    if (!mulib_sstore_dir_(d) || !mulib_sstore_name_(n) || !mulib_sc_view_ok(w))
        return MULIB_SC_INVALID;
    if (w.len > MULIB_SSTORE_BLOB_MAX_)
        return MULIB_SC_LIMIT;

    struct stat st;
    int r = mulib_sstore_target_(d, n, &st);
    if (r != MULIB_SC_OK && r != MULIB_SC_NOTFOUND)
        return r;
    unsigned char bytes[16];
    r = mulib_scr_random(p, bytes, sizeof bytes);
    if (r != MULIB_SC_OK)
        return r;
    char tmp[38] = ".tmp-";
    const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < sizeof bytes; ++i) {
        tmp[5 + 2 * i] = hex[bytes[i] >> 4];
        tmp[6 + 2 * i] = hex[bytes[i] & 15];
    }
    int fd = openat(d, tmp, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0)
        return MULIB_SC_IO;
    /* Keep the blob readable by its owner even with an unusual umask. */
    if (fchmod(fd, 0600) != 0)
        r = MULIB_SC_IO;
    size_t pos = 0;
    while (r == MULIB_SC_OK && pos < w.len) {
        ssize_t z = write(fd, w.ptr + pos, w.len - pos);
        if (z < 0 && errno == EINTR)
            continue;
        if (z <= 0) {
            r = MULIB_SC_IO;
            break;
        }
        pos += (size_t)z;
    }
    if (r == MULIB_SC_OK && mulib_sstore_sync_(fd) != 0)
        r = MULIB_SC_IO;
    if (close(fd) != 0 && r == MULIB_SC_OK)
        r = MULIB_SC_IO;
    if (r == MULIB_SC_OK && renameat(d, tmp, d, n) != 0)
        r = errno == EINTR || errno == EIO ? MULIB_SC_UNCERTAIN : MULIB_SC_IO;
    if (r != MULIB_SC_OK) {
        if (r != MULIB_SC_UNCERTAIN)
            (void)unlinkat(d, tmp, 0);
        return r;
    }
    return mulib_sstore_sync_(d) ? MULIB_SC_UNCERTAIN : MULIB_SC_OK;
}

int mulib_sstore_remove(int d, const char *n)
{
    if (!mulib_sstore_dir_(d) || !mulib_sstore_name_(n))
        return MULIB_SC_INVALID;
    struct stat st;
    int r = mulib_sstore_target_(d, n, &st);
    if (r != MULIB_SC_OK)
        return r;
    if (unlinkat(d, n, 0) != 0)
        return errno == EINTR || errno == EIO ? MULIB_SC_UNCERTAIN : mulib_sstore_errno_(errno);
    return mulib_sstore_sync_(d) ? MULIB_SC_UNCERTAIN : MULIB_SC_OK;
}
#undef MULIB_SSTORE_BLOB_MAX_
#else
int mulib_sstore_read(int d, const char *n, mulib_sc_buf o, size_t *w)
{
    (void)d; (void)n; (void)o; (void)w;
    return MULIB_SC_UNSUPPORTED;
}
int mulib_sstore_write(const mulib_scr_provider *p, int d, const char *n, mulib_sc_view w)
{
    (void)p; (void)d; (void)n; (void)w;
    return MULIB_SC_UNSUPPORTED;
}
int mulib_sstore_remove(int d, const char *n)
{
    (void)d; (void)n;
    return MULIB_SC_UNSUPPORTED;
}
#endif
#ifdef __cplusplus
}
#endif
#endif
