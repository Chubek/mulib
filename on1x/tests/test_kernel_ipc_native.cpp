#include "on1x_kernel_ipc.h"
#include <mulib-ipc-framed.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <thread>
#include <time.h>

static bool fail_socket, fail_box, expire_clock, fail_second_init, fail_shutdown, empty_frame;
static int init_calls, invocations, session_calls, releases, captured_fds[2] = {-1, -1};
static int close_calls[2];
static on1x_unique *active_session;
static on1x_unique *nested_pair, *queued_pair;
static std::thread::id opening_thread;
static int64_t expected_byte;
static time_t tick;

static void check_at(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "framed private ABI check failed at line %d\n", line);
        std::abort();
    }
}
#define check(condition) check_at((condition), __LINE__)

static int open_fds() {
    DIR *dir = opendir("/proc/self/fd");
    check(dir != nullptr);
    int count = 0;
    while (readdir(dir)) ++count;
    check(closedir(dir) == 0);
    return count;
}

extern "C" {
int __real_socketpair(int, int, int, int *);
int __wrap_socketpair(int family, int type, int protocol, int *fds) {
    if (fail_socket) {
        errno = EMFILE;
        return -1;
    }
    const int status = __real_socketpair(family, type, protocol, fds);
    if (!status) {
        for (int i = 0; i < 2; ++i) {
            captured_fds[i] = fds[i];
            close_calls[i] = 0;
            const int flags = fcntl(fds[i], F_GETFD);
            check(flags >= 0 && (flags & FD_CLOEXEC) != 0);
        }
    }
    return status;
}

int __real_close(int);
int __wrap_close(int fd) {
    for (int i = 0; i < 2; ++i)
        if (fd == captured_fds[i]) ++close_calls[i];
    return __real_close(fd);
}

int __real_shutdown(int, int);
int __wrap_shutdown(int fd, int how) {
    if (fail_shutdown) {
        errno = EIO;
        return -1;
    }
    return __real_shutdown(fd, how);
}

int __real_mulib_ipc_framed_init(mulib_ipc_framed_t *, int, uint32_t);
int __wrap_mulib_ipc_framed_init(mulib_ipc_framed_t *s, int fd, uint32_t max) {
    if (fail_second_init && ++init_calls == 2) return MULIB_SB_SYSTEM;
    return __real_mulib_ipc_framed_init(s, fd, max);
}

int __real_mulib_ipc_send(mulib_ipc_framed_t *, const void *, size_t, int64_t);
int __wrap_mulib_ipc_send(mulib_ipc_framed_t *s, const void *data, size_t n, int64_t deadline) {
    if (empty_frame) return __real_mulib_ipc_send(s, nullptr, 0, deadline);
    const int status = __real_mulib_ipc_send(s, data, n, deadline);
    if (status == 0) {
        // Independent wire oracle, without consuming the reader's frame.
        unsigned char frame[5] = {};
        check(recv(captured_fds[1], frame, sizeof frame, MSG_PEEK | MSG_DONTWAIT)
              == static_cast<ssize_t>(sizeof frame));
        check(frame[0] == 0 && frame[1] == 0 && frame[2] == 0 &&
              frame[3] == 1 && frame[4] == expected_byte);
    }
    return status;
}

on1x_unique *__real_on1x_unique_try_box(void *, size_t, on1x_unique_destructor);
on1x_unique *__wrap_on1x_unique_try_box(void *source, size_t size, on1x_unique_destructor drop) {
    if (fail_box) return nullptr;
    return __real_on1x_unique_try_box(source, size, drop);
}

int __real_clock_gettime(clockid_t, struct timespec *);
int __wrap_clock_gettime(clockid_t clock, struct timespec *out) {
    if (!expire_clock) return __real_clock_gettime(clock, out);
    out->tv_sec = tick;
    out->tv_nsec = 0;
    tick += 3;
    return 0;
}

int64_t on_byte(on1x_env *env, int64_t byte) {
    check(env != nullptr && byte == expected_byte && std::this_thread::get_id() == opening_thread);
    check(on1x_unique_live_allocations() == 0);
    for (int fd : captured_fds) check(fcntl(fd, F_GETFD) == -1 && errno == EBADF);
    check(close_calls[0] == 1 && close_calls[1] == 1);
    ++invocations;
    return byte - 42;
}

void release_context(on1x_env *) {
    check(std::this_thread::get_id() == opening_thread);
    ++releases;
}

int64_t session_byte(on1x_env *env, int64_t byte) {
    check(byte == expected_byte && std::this_thread::get_id() == opening_thread);
    if (env) check(env->references == 1);
    check(on1x_kernel_ipc_v1_send(active_session, 0, 1000) == MULIB_SB_STATE);
    check(on1x_kernel_ipc_v1_revoke(active_session) == MULIB_SB_STATE);
    check(on1x_kernel_ipc_v1_finish_send(active_session) == MULIB_SB_STATE);
    check(on1x_kernel_ipc_v1_register_callback(active_session, session_byte, env) == MULIB_SB_STATE);
    int64_t out = 777;
    check(on1x_kernel_ipc_v1_poll(active_session, 1000, &out) == MULIB_SB_STATE && out == 777);
    ++session_calls;
    return byte - 42;
}

int64_t nested_byte(on1x_env *, int64_t byte) {
    check(byte == expected_byte && nested_pair == nullptr);
    // The generic runtime is still draining the outer drop, so the consumed
    // box is queued but cannot be used and both sockets must already be closed.
    check(on1x_unique_live_allocations() == 2);
    for (int fd : captured_fds) check(fcntl(fd, F_GETFD) == -1 && errno == EBADF);
    int64_t output = 777;
    check(on1x_kernel_ipc_v1_roundtrip(&queued_pair, 0, 1000, nested_byte, nullptr, &output)
          == MULIB_SB_STATE && output == 777 && queued_pair != nullptr);
    return -42;
}

void drop_outer(void *) {
    int64_t output = 777;
    queued_pair = nested_pair;
    check(on1x_kernel_ipc_v1_roundtrip(&nested_pair, 0, 1000, nested_byte, nullptr, &output)
          == MULIB_SB_OK && output == -42 && nested_pair == nullptr);
    queued_pair = nullptr;
}

void drop_unrelated(void *) {}
}

static void test_nested_cleanup(int baseline) {
    check(on1x_kernel_ipc_v1_open_pair(&nested_pair) == MULIB_SB_OK);
    int dummy = 0;
    on1x_unique *outer = on1x_unique_try_box(&dummy, sizeof dummy, drop_outer);
    check(outer != nullptr);
    expected_byte = 0;
    on1x_unique_drop(outer);
    check(nested_pair == nullptr && open_fds() == baseline && on1x_unique_live_allocations() == 0);
    check(close_calls[0] == 1 && close_calls[1] == 1);
}

static void test_wrong_owner(int baseline) {
    on1x_unique *pair = nullptr;
    on1x_unique *session = nullptr;
    int dummy = 0;
    on1x_unique *unrelated = on1x_unique_try_box(&dummy, sizeof dummy, drop_unrelated);
    check(unrelated != nullptr);
    check(on1x_kernel_ipc_v1_open_pair(&pair) == MULIB_SB_OK);
    check(on1x_kernel_ipc_v1_open_session(&session) == MULIB_SB_OK);
    int64_t output = 777;
    check(on1x_kernel_ipc_v1_poll(pair, 1000, &output) == MULIB_SB_STATE && output == 777);
    check(on1x_kernel_ipc_v1_revoke(unrelated) == MULIB_SB_STATE);
    on1x_unique *slot = session;
    check(on1x_kernel_ipc_v1_roundtrip(&slot, 0, 1000, nested_byte, nullptr, &output)
          == MULIB_SB_STATE && slot == session && output == 777);
    slot = unrelated;
    check(on1x_kernel_ipc_v1_roundtrip(&slot, 0, 1000, nested_byte, nullptr, &output)
          == MULIB_SB_STATE && slot == unrelated && output == 777);
    std::thread foreign([pair] {
        auto copy = pair;
        int64_t out = 888;
        check(on1x_kernel_ipc_v1_roundtrip(&copy, 0, 1000, nested_byte, nullptr, &out)
              == MULIB_SB_STATE && copy == pair && out == 888);
    });
    foreign.join();
    check(open_fds() == baseline + 4 && on1x_unique_live_allocations() == 3);
    on1x_unique_drop(unrelated);
    on1x_unique_drop(pair);
    on1x_unique_drop(session);
    check(open_fds() == baseline && on1x_unique_live_allocations() == 0);
    check(close_calls[0] == 1 && close_calls[1] == 1);
}

static void register_owned() {
    auto *env = on1x_env_alloc(sizeof(on1x_env), release_context);
    check(on1x_kernel_ipc_v1_register_callback(active_session, session_byte, env) == MULIB_SB_OK);
    check(on1x_kernel_ipc_v1_register_callback(active_session, session_byte, env) == MULIB_SB_STATE);
    check(env->references == 2);
    on1x_env_release(env);
}

static void clear_session(int baseline) {
    on1x_unique_drop(active_session);
    active_session = nullptr;
    check(open_fds() == baseline && on1x_unique_live_allocations() == 0);
}

static void test_session(int baseline) {
    check(on1x_kernel_ipc_v1_open_session(nullptr) == MULIB_SB_INVALID);
    fail_socket = true;
    check(on1x_kernel_ipc_v1_open_session(&active_session) == MULIB_SB_SYSTEM);
    fail_socket = false;
    fail_second_init = true;
    init_calls = 0;
    check(on1x_kernel_ipc_v1_open_session(&active_session) == MULIB_SB_SYSTEM);
    fail_second_init = false;
    fail_box = true;
    check(on1x_kernel_ipc_v1_open_session(&active_session) == MULIB_SB_NOMEM);
    fail_box = false;
    check(!active_session && open_fds() == baseline);

    check(on1x_kernel_ipc_v1_open_session(&active_session) == MULIB_SB_OK);
    int64_t out = 999;
    check(on1x_kernel_ipc_v1_send(active_session, 0, 1000) == MULIB_SB_STATE);
    check(on1x_kernel_ipc_v1_register_callback(active_session, nullptr, nullptr) == MULIB_SB_INVALID);
    register_owned();
    check(on1x_kernel_ipc_v1_poll(active_session, 1000, &out) == MULIB_SB_AGAIN && out == 999);
    check(on1x_kernel_ipc_v1_send(active_session, 42, 0) == MULIB_SB_INVALID);
    expected_byte = 0;
    check(on1x_kernel_ipc_v1_send(active_session, 0, 1000) == MULIB_SB_OK);
    std::thread foreign([] {
        int64_t value = 999;
        check(on1x_kernel_ipc_v1_poll(active_session, 1000, &value) == MULIB_SB_STATE && value == 999);
        check(on1x_kernel_ipc_v1_revoke(active_session) == MULIB_SB_STATE);
        check(on1x_kernel_ipc_v1_register_callback(active_session, session_byte, nullptr) == MULIB_SB_STATE);
        check(on1x_kernel_ipc_v1_send(active_session, 0, 1000) == MULIB_SB_STATE);
        check(on1x_kernel_ipc_v1_finish_send(active_session) == MULIB_SB_STATE);
    });
    foreign.join();
    check(session_calls == 0 && releases == 0);
    for (int i = 0; i < 256; ++i) {
        expected_byte = i;
        if (i != 0)
            check(on1x_kernel_ipc_v1_send(active_session, static_cast<uint8_t>(i), 1000) == MULIB_SB_OK);
        check(on1x_kernel_ipc_v1_send(active_session, 0, 1000) == MULIB_SB_AGAIN);
        check(on1x_kernel_ipc_v1_poll(active_session, 1001, &out) == MULIB_SB_INVALID);
        check(on1x_kernel_ipc_v1_poll(active_session, 1000, &out) == MULIB_SB_OK && out == i - 42);
        check(session_calls == i + 1 && releases == 0 && open_fds() == baseline + 2);
    }
    check(on1x_kernel_ipc_v1_send(active_session, 255, 1000) == MULIB_SB_OK);
    check(on1x_kernel_ipc_v1_revoke(active_session) == MULIB_SB_OK && releases == 1);
    check(on1x_kernel_ipc_v1_revoke(active_session) == MULIB_SB_OK && releases == 1);
    register_owned();
    out = 999;
    check(on1x_kernel_ipc_v1_poll(active_session, 1000, &out) == MULIB_SB_DENIED && out == 999);
    check(session_calls == 256 && releases == 1);
    check(on1x_kernel_ipc_v1_send(active_session, 255, 1000) == MULIB_SB_OK);
    check(on1x_kernel_ipc_v1_finish_send(active_session) == MULIB_SB_OK);
    check(on1x_kernel_ipc_v1_finish_send(active_session) == MULIB_SB_OK);
    check(on1x_kernel_ipc_v1_send(active_session, 255, 1000) == MULIB_SB_STATE);
    check(on1x_kernel_ipc_v1_poll(active_session, 1000, &out) == MULIB_SB_OK && out == 213);
    check(on1x_kernel_ipc_v1_poll(active_session, 1000, &out) == MULIB_SB_EOF && out == 213);
    check(on1x_kernel_ipc_v1_poll(active_session, 1000, &out) == MULIB_SB_STATE);
    check(releases == 2 && session_calls == 257 && open_fds() == baseline);
    clear_session(baseline);
    check(close_calls[0] == 1 && close_calls[1] == 1);

    for (int fault = 0; fault < 6; ++fault) {
        check(on1x_kernel_ipc_v1_open_session(&active_session) == MULIB_SB_OK);
        register_owned();
        expected_byte = 0;
        out = 999;
        if (fault == 0) {
            expire_clock = true;
            check(on1x_kernel_ipc_v1_send(active_session, 0, 100) == MULIB_SB_TIMEOUT);
            expire_clock = false;
        } else if (fault == 1) {
            check(on1x_kernel_ipc_v1_send(active_session, 0, 1000) == MULIB_SB_OK);
            expire_clock = true;
            check(on1x_kernel_ipc_v1_poll(active_session, 100, &out) == MULIB_SB_TIMEOUT);
            expire_clock = false;
        } else if (fault == 2) {
            empty_frame = true;
            check(on1x_kernel_ipc_v1_send(active_session, 0, 1000) == MULIB_SB_OK);
            empty_frame = false;
            check(on1x_kernel_ipc_v1_poll(active_session, 1000, &out) == MULIB_SB_PROTOCOL);
        } else if (fault == 3) {
            fail_shutdown = true;
            check(on1x_kernel_ipc_v1_finish_send(active_session) == MULIB_SB_SYSTEM);
            fail_shutdown = false;
        } else if (fault == 4) {
            check(on1x_kernel_ipc_v1_send(active_session, 0, 1000) == MULIB_SB_OK);
            clear_session(baseline); // Drops a registered callback with a pending byte.
        } else {
            check(on1x_kernel_ipc_v1_finish_send(active_session) == MULIB_SB_OK);
            check(on1x_kernel_ipc_v1_poll(active_session, 1000, &out) == MULIB_SB_EOF);
        }
        check(out == 999 && releases == fault + 3 && session_calls == 257 && open_fds() == baseline);
        if (active_session) {
            check(on1x_kernel_ipc_v1_revoke(active_session) == MULIB_SB_STATE);
            clear_session(baseline);
        }
        check(close_calls[0] == 1 && close_calls[1] == 1);
    }
}

int main() {
    opening_thread = std::this_thread::get_id();
    const int baseline = open_fds();
    on1x_unique *pair = nullptr;
    on1x_env context = {};
    check(on1x_kernel_ipc_v1_open_pair(nullptr) == MULIB_SB_INVALID);
    fail_socket = true;
    check(on1x_kernel_ipc_v1_open_pair(&pair) == MULIB_SB_SYSTEM && !pair && open_fds() == baseline);
    fail_socket = false;
    fail_second_init = true;
    check(on1x_kernel_ipc_v1_open_pair(&pair) == MULIB_SB_SYSTEM && !pair && open_fds() == baseline);
    fail_second_init = false;
    fail_box = true;
    check(on1x_kernel_ipc_v1_open_pair(&pair) == MULIB_SB_NOMEM && !pair && open_fds() == baseline);
    fail_box = false;

    for (int i = 0; i < 256; ++i) {
        expected_byte = i;
        int64_t out = -999;
        check(on1x_kernel_ipc_v1_open_pair(&pair) == 0 && pair && open_fds() == baseline + 2);
        check(on1x_kernel_ipc_v1_roundtrip(&pair, static_cast<uint8_t>(i), 1000,
                                           on_byte, &context, &out) == 0);
        check(out == i - 42 && !pair && invocations == i + 1 && open_fds() == baseline);
        out = 999;
        check(on1x_kernel_ipc_v1_roundtrip(&pair, 0, 1000, on_byte, &context, &out) == MULIB_SB_INVALID);
        check(out == 999 && invocations == i + 1);
    }

    int64_t out = 999;
    check(on1x_kernel_ipc_v1_open_pair(&pair) == 0 && pair);
    expire_clock = true;
    check(on1x_kernel_ipc_v1_roundtrip(&pair, 0, 100, on_byte, &context, &out) == MULIB_SB_TIMEOUT);
    expire_clock = false;
    check(!pair && out == 999 && invocations == 256 && open_fds() == baseline);
    check(on1x_kernel_ipc_v1_open_pair(&pair) == 0 && pair);
    check(on1x_kernel_ipc_v1_roundtrip(&pair, 0, INT64_MAX, on_byte, &context, &out) == MULIB_SB_INVALID);
    check(!pair && out == 999 && invocations == 256 && open_fds() == baseline);
    check(on1x_kernel_ipc_v1_open_pair(&pair) == 0 && pair);
    check(on1x_kernel_ipc_v1_roundtrip(&pair, 0, 1000, nullptr, &context, &out) == MULIB_SB_INVALID);
    check(!pair && out == 999 && invocations == 256 && open_fds() == baseline);
    check(close_calls[0] == 1 && close_calls[1] == 1);
    check(on1x_kernel_ipc_v1_open_pair(&pair) == 0 && pair);
    empty_frame = true;
    check(on1x_kernel_ipc_v1_roundtrip(&pair, 0, 1000, on_byte, &context, &out) == MULIB_SB_PROTOCOL);
    empty_frame = false;
    check(!pair && out == 999 && invocations == 256 && open_fds() == baseline);
    check(close_calls[0] == 1 && close_calls[1] == 1);
    check(on1x_kernel_ipc_v1_open_pair(&pair) == 0 && pair);
    on1x_unique_drop(pair);
    check(open_fds() == baseline && on1x_unique_live_allocations() == 0);

    on1x_unique *pool[128] = {};
    for (auto &item : pool) check(on1x_kernel_ipc_v1_open_pair(&item) == 0 && item);
    pair = nullptr;
    check(open_fds() == baseline + 256);
    check(on1x_kernel_ipc_v1_open_pair(&pair) == MULIB_SB_LIMIT && !pair);
    check(on1x_kernel_ipc_v1_open_session(&pair) == MULIB_SB_LIMIT && !pair);
    for (auto item : pool) on1x_unique_drop(item);
    check(open_fds() == baseline && on1x_unique_live_allocations() == 0);
    check(on1x_kernel_ipc_v1_open_pair(&pair) == 0 && pair);
    on1x_unique_drop(pair);
    test_nested_cleanup(baseline);
    test_wrong_owner(baseline);
    test_session(baseline);
}
