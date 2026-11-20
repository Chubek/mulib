#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64
#include "container/runtime.h"
#include "mulib-xthreads.h"
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

enum { WORKERS = 8, ROUNDS = 1000 };
static pthread_barrier_t gate;
static bool fail_key;
static _Thread_local bool fail_slot;
static enum { NO_FAULT, STAT_FAULT, READ_FAULT, CLOSE_FAULT } file_fault;
static unsigned closes;
static bool file_call;

int __real_mulib_xtss_create(mulib_xtss_t *);
int __real_mulib_xtss_set(mulib_xtss_t, void *);
int __real_fstat64(int, struct stat *);
ssize_t __real_pread64(int, void *, size_t, off_t);
int __real_close(int);

int __wrap_mulib_xtss_create(mulib_xtss_t *key)
{
    return fail_key ? MULIB_XTHREAD_SYSTEM : __real_mulib_xtss_create(key);
}

int __wrap_mulib_xtss_set(mulib_xtss_t key, void *value)
{
    return fail_slot ? MULIB_XTHREAD_SYSTEM : __real_mulib_xtss_set(key, value);
}

int __wrap_fstat64(int fd, struct stat *info)
{
    if (file_call && file_fault == STAT_FAULT) {
        errno = EIO;
        return -1;
    }
    return __real_fstat64(fd, info);
}

ssize_t __wrap_pread64(int fd, void *out, size_t bytes, off_t offset)
{
    assert(file_call && bytes == 1);
    int flags = fcntl(fd, F_GETFD);
    assert(flags >= 0 && (flags & FD_CLOEXEC));
    if (file_fault == READ_FAULT) {
        errno = EIO;
        return -1;
    }
    return __real_pread64(fd, out, bytes, offset);
}

int __wrap_close(int fd)
{
    int rc = __real_close(fd);
    if (file_call) {
        ++closes;
        if (file_fault == CLOSE_FAULT) {
            /* On Linux even failed close releases the descriptor. */
            assert(rc == 0);
            errno = EINTR;
            return -1;
        }
    }
    return rc;
}

static void synchronize(void)
{
    int rc = pthread_barrier_wait(&gate);
    assert(rc == 0 || rc == PTHREAD_BARRIER_SERIAL_THREAD);
}

static int worker(void *arg)
{
    const int64_t id = *(const int64_t *)arg;
    synchronize(); /* First use of the process key is concurrent. */
    assert(!on1x_container_tls_i64_present());
    fail_slot = true;
    assert(!on1x_container_tls_i64_set(id));
    assert(!on1x_container_tls_i64_present());
    fail_slot = false;
    assert(on1x_container_tls_i64_set(id));
    synchronize(); /* All initially distinct values are now live. */
    assert(on1x_container_tls_i64_present());
    assert(on1x_container_tls_i64_value() == id);
    fail_slot = true; /* Once a native slot is bound, updates cannot rebind it. */
    for (int i = 0; i < ROUNDS; ++i) {
        assert(on1x_container_tls_i64_set(id * 1000 + i));
        assert(on1x_container_tls_i64_value() == id * 1000 + i);
        on1x_container_tls_i64_clear();
        assert(!on1x_container_tls_i64_present());
    }
    /* Leave a live value when the native thread exits: domain owns no heap. */
    assert(on1x_container_tls_i64_set(id));
    return 0;
}

static void test_threads(void)
{
    int64_t ids[WORKERS];
    mulib_xthread_t threads[WORKERS];
    assert(pthread_barrier_init(&gate, NULL, WORKERS) == 0);
    for (int i = 0; i < WORKERS; ++i) {
        ids[i] = 1 + i;
        assert(mulib_xthread_create(&threads[i], worker, &ids[i]) == MULIB_XTHREAD_OK);
    }
    for (int i = 0; i < WORKERS; ++i) {
        int status = -1;
        assert(mulib_xthread_join(threads[i], &status) == MULIB_XTHREAD_OK && status == 0);
    }
    assert(!on1x_container_tls_i64_present());
    assert(pthread_barrier_destroy(&gate) == 0);
}

static unsigned fd_count(void)
{
    DIR *dir = opendir("/proc/self/fd");
    assert(dir);
    unsigned count = 0;
    while (readdir(dir))
        ++count;
    assert(closedir(dir) == 0);
    return count;
}

static void read_case(const char *path, int64_t offset, int64_t expected, unsigned want_closes)
{
    closes = 0;
    file_call = true;
    assert(on1x_container_file_byte_at(path, offset) == expected);
    file_call = false;
    assert(closes == want_closes);
}

static void test_file(void)
{
    const unsigned initial_fds = fd_count();
    char long_path[4097];
    memset(long_path, 'x', sizeof long_path - 1);
    long_path[sizeof long_path - 1] = '\0';
    read_case(NULL, 0, -1, 0);
    read_case("", 0, -1, 0);
    read_case(long_path, 0, -1, 0);
    read_case("bytes", -1, -1, 0);
    read_case("bytes", INT64_MAX, -1, 0);
    read_case("absent", 0, -2, 0);
    read_case("link", 0, -2, 0);
    read_case("fifo", 0, -3, 1);
    read_case(".", 0, -3, 1);
    read_case("bytes", 0, 0, 1);
    read_case("bytes", 1, 255, 1);
    read_case("bytes", 3, 256, 1);
    read_case("sparse", INT64_C(4294967303), 173, 1);
    for (int i = 0; i < ROUNDS; ++i)
        read_case("bytes", 2, 42, 1);
    file_fault = STAT_FAULT;
    read_case("bytes", 0, -4, 1);
    file_fault = READ_FAULT;
    read_case("bytes", 0, -4, 1);
    file_fault = CLOSE_FAULT;
    read_case("bytes", 0, -5, 1);
    assert(fd_count() == initial_fds);
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "fail-key") == 0) {
        fail_key = true;
        assert(!on1x_container_tls_i64_set(37));
        assert(!on1x_container_tls_i64_present());
        on1x_container_tls_i64_clear();
        fail_key = false;
        /* Poisoned initialization is deterministic and never retried. */
        assert(!on1x_container_tls_i64_set(38));
        return 0;
    }
    test_threads();
    test_threads(); /* Also checks no stale values on recycled native threads. */
    test_file();
    return 0;
}
