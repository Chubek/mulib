#include "on1x_kernel_ipc.h"
#include <limits.h>
#include <mulib-ipc-framed.h>
#include <sys/socket.h>
#include <unistd.h>

_Static_assert(CHAR_BIT == 8 && sizeof(void *) == 8 && _Alignof(void *) == 8 &&
               sizeof(int) == 4 && _Alignof(int) == 4 &&
               sizeof(size_t) == 8 && _Alignof(size_t) == 8 &&
               sizeof(int64_t) == 8 && _Alignof(int64_t) == 8 &&
               sizeof(uint64_t) == 8 && _Alignof(uint64_t) == 8 &&
               sizeof(uint32_t) == 4 && _Alignof(uint32_t) == 4 &&
               sizeof(uint8_t) == 1 && _Alignof(uint8_t) == 1,
               "framed KernelBind platform ABI drift");
_Static_assert(_Generic(&mulib_ipc_framed_init,
               int (*)(mulib_ipc_framed_t *, int, uint32_t): 1, default: 0) &&
               _Generic(&mulib_ipc_deadline, int (*)(uint64_t, int64_t *): 1, default: 0) &&
               _Generic(&mulib_ipc_send,
               int (*)(mulib_ipc_framed_t *, const void *, size_t, int64_t): 1, default: 0) &&
               _Generic(&mulib_ipc_receive,
               int (*)(mulib_ipc_framed_t *, void *, size_t, size_t *, int64_t): 1, default: 0),
               "framed KernelBind prototype drift");
_Static_assert(sizeof(mulib_ipc_framed_t) == 12 && _Alignof(mulib_ipc_framed_t) == 4 &&
               offsetof(mulib_ipc_framed_t, fd) == 0 &&
               offsetof(mulib_ipc_framed_t, poisoned) == 4 &&
               offsetof(mulib_ipc_framed_t, max_frame) == 8 &&
               _Generic(((mulib_ipc_framed_t *)0)->fd, int: 1, default: 0) &&
               _Generic(((mulib_ipc_framed_t *)0)->poisoned, int: 1, default: 0) &&
               _Generic(((mulib_ipc_framed_t *)0)->max_frame, uint32_t: 1, default: 0),
               "framed KernelBind record ABI drift");
_Static_assert(MULIB_SB_OK == 0 && MULIB_SB_INVALID == -1 && MULIB_SB_SYSTEM == -2 &&
               MULIB_SB_LIMIT == -5 && MULIB_SB_TIMEOUT == -6 &&
               MULIB_SB_PROTOCOL == -7 && MULIB_SB_NOMEM == -8 && MULIB_SB_EOF == -10 &&
               MULIB_SB_AGAIN == 1 && MULIB_SB_DENIED == -4 && MULIB_SB_STATE == -9,
               "framed KernelBind status drift");

typedef struct { int fds[2]; mulib_ipc_framed_t writer, reader; } ipc_pair;
enum { MAX_LIVE_PAIRS = 128 };
static unsigned live_pairs;

static void close_pair(ipc_pair *pair) {
    // On Linux close must not be retried (even on EINTR): a new descriptor
    // could be using that number. Socket teardown is not application I/O.
    for (size_t i = 0; i < 2; ++i) {
        if (pair->fds[i] >= 0) (void)close(pair->fds[i]);
        pair->fds[i] = -1;
    }
}

static void drop_pair(void *payload) {
    close_pair(payload);
    --live_pairs;
}

static int make_pair(ipc_pair *pair) {
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, pair->fds) != 0)
        return MULIB_SB_SYSTEM;
    int status = mulib_ipc_framed_init(&pair->writer, pair->fds[0], 1);
    if (status == 0) status = mulib_ipc_framed_init(&pair->reader, pair->fds[1], 1);
    if (status != 0) close_pair(pair);
    return status;
}

static int publish_owner(void *data, size_t size, on1x_unique_destructor drop,
                         on1x_unique **out) {
    on1x_unique *owner = on1x_unique_try_box(data, size, drop);
    if (!owner) return MULIB_SB_NOMEM;
    ++live_pairs;
    *out = owner;
    return MULIB_SB_OK;
}

int64_t on1x_kernel_ipc_v1_open_pair(on1x_unique **out) {
    ipc_pair pair = {0};
    if (!out) return MULIB_SB_INVALID;
    if (live_pairs >= MAX_LIVE_PAIRS) return MULIB_SB_LIMIT;
    int status = make_pair(&pair);
    if (status != 0) return status;
    status = publish_owner(&pair, sizeof pair, drop_pair, out);
    if (status != 0) close_pair(&pair);
    return status;
}

int64_t on1x_kernel_ipc_v1_roundtrip(on1x_unique **slot, uint8_t byte,
        int64_t timeout_ms, on1x_kernel_ipc_callback callback,
        on1x_env *context, int64_t *out) {
    if (!slot || !*slot) return MULIB_SB_INVALID;
    on1x_unique *owner = *slot;
    *slot = NULL;
    int status = MULIB_SB_INVALID;
    uint8_t received = 0;
    if (callback && out && timeout_ms >= 1 && timeout_ms <= 1000) {
        ipc_pair *pair = on1x_unique_borrow(owner, sizeof *pair, __FILE__, __LINE__, 1);
        int64_t deadline = 0;
        size_t length = 0;
        status = mulib_ipc_deadline((uint64_t)timeout_ms, &deadline);
        if (status == 0) status = mulib_ipc_send(&pair->writer, &byte, sizeof byte, deadline);
        if (status == 0) status = mulib_ipc_receive(&pair->reader, &received, sizeof received,
                                                     &length, deadline);
        if (status == 0 && length != sizeof received) status = MULIB_SB_PROTOCOL;
    }
    on1x_unique_drop(owner);
    if (status == 0) *out = callback(context, received);
    return status;
}

typedef struct {
    ipc_pair pair;
    const void *thread;
    on1x_kernel_ipc_callback callback;
    on1x_env *context;
    bool pending, deliver, writing_finished, in_callback;
} ipc_session;

// Only compared, never dereferenced. Source never transfers a box between
// threads; this catches an accidental private-ABI call on another live thread.
static _Thread_local unsigned char thread_marker;

static void release_callback(ipc_session *session) {
    on1x_env *context = session->context;
    session->callback = NULL;
    session->context = NULL;
    session->deliver = false;
    on1x_env_release(context);
}

static void close_session(ipc_session *session) {
    close_pair(&session->pair);
    session->pending = false;
    release_callback(session);
}

static void drop_session(void *payload) {
    close_session(payload);
    --live_pairs;
}

static ipc_session *get_session(on1x_unique *owner) {
    if (!owner) return NULL;
    ipc_session *session = on1x_unique_borrow(owner, sizeof *session, __FILE__, __LINE__, 1);
    if (session->thread != &thread_marker || session->in_callback || session->pair.fds[0] < 0)
        return NULL;
    return session;
}

int64_t on1x_kernel_ipc_v1_open_session(on1x_unique **out) {
    ipc_session session = {.thread = &thread_marker};
    if (!out) return MULIB_SB_INVALID;
    if (live_pairs >= MAX_LIVE_PAIRS) return MULIB_SB_LIMIT;
    int status = make_pair(&session.pair);
    if (status != 0) return status;
    status = publish_owner(&session, sizeof session, drop_session, out);
    if (status != 0) close_pair(&session.pair);
    return status;
}

int64_t on1x_kernel_ipc_v1_register_callback(on1x_unique *owner,
        on1x_kernel_ipc_callback callback, on1x_env *context) {
    if (!callback) return MULIB_SB_INVALID;
    ipc_session *session = get_session(owner);
    if (!session || session->writing_finished || session->callback) return MULIB_SB_STATE;
    on1x_env_retain(context);
    session->context = context;
    session->callback = callback;
    return MULIB_SB_OK;
}

int64_t on1x_kernel_ipc_v1_revoke(on1x_unique *owner) {
    ipc_session *session = get_session(owner);
    if (!session) return MULIB_SB_STATE;
    release_callback(session);
    return MULIB_SB_OK;
}

int64_t on1x_kernel_ipc_v1_send(on1x_unique *owner, uint8_t byte, int64_t timeout_ms) {
    if (timeout_ms < 1 || timeout_ms > 1000) return MULIB_SB_INVALID;
    ipc_session *session = get_session(owner);
    if (!session || session->writing_finished || !session->callback) return MULIB_SB_STATE;
    if (session->pending) return MULIB_SB_AGAIN;
    int64_t deadline = 0;
    int status = mulib_ipc_deadline((uint64_t)timeout_ms, &deadline);
    if (status == 0) status = mulib_ipc_send(&session->pair.writer, &byte, sizeof byte, deadline);
    if (status == 0) {
        session->pending = true;
        session->deliver = true;
    } else close_session(session);
    return status;
}

int64_t on1x_kernel_ipc_v1_poll(on1x_unique *owner, int64_t timeout_ms, int64_t *out) {
    if (!out || timeout_ms < 1 || timeout_ms > 1000) return MULIB_SB_INVALID;
    ipc_session *session = get_session(owner);
    if (!session) return MULIB_SB_STATE;
    if (!session->pending && !session->writing_finished) return MULIB_SB_AGAIN;
    int64_t deadline = 0;
    size_t length = 0;
    uint8_t byte = 0;
    int status = mulib_ipc_deadline((uint64_t)timeout_ms, &deadline);
    if (status == 0)
        status = mulib_ipc_receive(&session->pair.reader, &byte, sizeof byte, &length, deadline);
    if (status == 0 && (!session->pending || length != sizeof byte)) status = MULIB_SB_PROTOCOL;
    if (status != 0) {
        close_session(session);
        return status;
    }
    session->pending = false;
    if (!session->deliver) return MULIB_SB_DENIED;
    session->deliver = false;
    session->in_callback = true;
    *out = session->callback(session->context, byte);
    session->in_callback = false;
    return MULIB_SB_OK;
}

int64_t on1x_kernel_ipc_v1_finish_send(on1x_unique *owner) {
    ipc_session *session = get_session(owner);
    if (!session) return MULIB_SB_STATE;
    if (session->writing_finished) return MULIB_SB_OK;
    if (shutdown(session->pair.fds[0], SHUT_WR) != 0) {
        close_session(session);
        return MULIB_SB_SYSTEM;
    }
    session->writing_finished = true;
    return MULIB_SB_OK;
}
