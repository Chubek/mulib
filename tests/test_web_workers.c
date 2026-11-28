#include <assert.h>
#include "mulib-web-agentcluster.h"
#include "mulib-web-locks.h"
#include "mulib-web-serviceworker.h"
#include "mulib-web-sharedworker.h"
#include "mulib-web-taskqueue.h"
#include "mulib-web-worker.h"
#include "mulib-web-workerclient.h"
#include "mulib-web-workerglobal.h"
#include "mulib-web-worklet.h"

#define COUNT(v) (sizeof(v) / sizeof((v)[0]))

typedef struct {
    mulib_web_worker_t *worker;
    unsigned starts, stops;
    int start_result;
} worker_context;

static int start_worker(void *opaque)
{
    worker_context *ctx = opaque;
    ctx->starts++;
    assert(ctx->worker->state == mulib_web_worker_NEW);
    assert(!mulib_web_worker_start(ctx->worker));
    assert(!mulib_web_worker_terminate(ctx->worker));
    return ctx->start_result;
}

static void stop_worker(void *opaque)
{
    worker_context *ctx = opaque;
    ctx->stops++;
    assert(ctx->worker->state == mulib_web_worker_STOPPED);
    assert(!mulib_web_worker_start(ctx->worker));
    assert(!mulib_web_worker_terminate(ctx->worker));
}

static void lifecycle(void)
{
    mulib_web_worker_t worker = {0};
    worker_context ctx = {&worker, 0, 0, 0};
    assert(!mulib_web_worker_start(NULL));
    assert(!mulib_web_worker_terminate(NULL));
    assert(!mulib_web_worker_start(&worker));
    worker.start = start_worker;
    worker.stop = stop_worker;
    worker.ctx = &ctx;
    assert(!mulib_web_worker_start(&worker));
    assert(ctx.starts == 1 && !ctx.stops);
    assert(worker.state == mulib_web_worker_NEW && !worker.busy);
    ctx.start_result = 7;
    assert(mulib_web_worker_start(&worker) == 7);
    assert(worker.state == mulib_web_worker_RUNNING && !worker.busy);
    assert(!mulib_web_worker_start(&worker) && ctx.starts == 2);
    assert(mulib_web_worker_terminate(&worker) && ctx.stops == 1);
    assert(!mulib_web_worker_terminate(&worker) && ctx.stops == 1);
    assert(!mulib_web_worker_start(&worker) && ctx.starts == 2);

    worker = (mulib_web_worker_t){mulib_web_worker_NEW, start_worker, stop_worker, &ctx, 0};
    assert(mulib_web_worker_terminate(&worker));
    assert(worker.state == mulib_web_worker_STOPPED && ctx.stops == 1);
    assert(!mulib_web_worker_start(&worker) && ctx.starts == 2);
    assert(!mulib_web_worker_terminate(&worker));

    worker = (mulib_web_worker_t){mulib_web_worker_NEW, start_worker, NULL, &ctx, 0};
    assert(mulib_web_worker_start(&worker) == 7 && ctx.starts == 3);
    assert(mulib_web_worker_terminate(&worker) && ctx.stops == 1);
}

static void scopes(void)
{
    const mulib_web_serviceworker_registration regs[] = {
        {muba_str("https://example.test/a"), 1, 10},
        {muba_str("https://example.test/a/b"), 2, 20},
        {muba_str("https://example.test/a/b"), 1, 30},
        {muba_str("https://example.test/a/b"), 1, 40},
        {muba_str("https://example.test/a/b#frag"), 1, 50},
        {muba_str("https://example.test/a/b?x=%23"), 1, 60},
        {muba_str("https://example.test/a/b?x=%23#frag"), 1, 70},
        {muba_str("https://example.test/a/b/"), 1, 0},
        {muba_str("https://example.test/a"), 0, 80},
        {{NULL, 1}, 1, 90},
        {{NULL, 0}, 1, 100},
    };
    assert(mulib_web_serviceworker_match(regs, COUNT(regs), 1,
        muba_str("https://example.test/a/b#frag")) == 30);
    assert(mulib_web_serviceworker_match(regs, COUNT(regs), 1,
        muba_str("https://example.test/a/b?x=%23#frag")) == 60);
    assert(mulib_web_serviceworker_match(regs, COUNT(regs), 1,
        muba_str("https://example.test/a/b/x")) == 30);
    assert(mulib_web_serviceworker_match(regs, COUNT(regs), 1,
        muba_str("https://example.test/abc")) == 10);
    assert(mulib_web_serviceworker_match(regs, COUNT(regs), 2,
        muba_str("https://example.test/a/b#frag")) == 20);
    assert(!mulib_web_serviceworker_match(regs, COUNT(regs), 0,
        muba_str("https://example.test/a/b")));
    assert(!mulib_web_serviceworker_match(regs, COUNT(regs), 1, muba_str("#frag")));
    assert(!mulib_web_serviceworker_match(regs, COUNT(regs), 1, (muba_span){NULL, 1}));
    assert(!mulib_web_serviceworker_match(NULL, COUNT(regs), 1, muba_str("x")));
}

typedef struct { unsigned calls; uintptr_t arg; int result; } worklet_context;

static int processor(void *opaque, uintptr_t arg)
{
    worklet_context *ctx = opaque;
    ctx->calls++;
    ctx->arg = arg;
    return ctx->result;
}

static void dispatch(void)
{
    const unsigned char key[] = {'a', 0, 'b'};
    muba_span name = {key, sizeof key};
    worklet_context a = {0, 0, -8}, b = {0, 0, 1};
    mulib_web_worklet_entry entries[] = {
        {{NULL, 0}, processor, &b},
        {muba_str("a"), processor, &b},
        {name, processor, &a},
        {muba_str("other"), processor, &b},
    };
    assert(mulib_web_worklet_run(entries, COUNT(entries), name, UINTPTR_MAX) == -8);
    assert(a.calls == 1 && a.arg == UINTPTR_MAX && !b.calls);
    a.result = 0;
    assert(!mulib_web_worklet_run(entries, COUNT(entries), name, 5));
    assert(a.calls == 2 && a.arg == 5 && !b.calls);
    entries[3].name = name;
    assert(!mulib_web_worklet_run(entries, COUNT(entries), name, 0));
    entries[3].fn = NULL;
    assert(!mulib_web_worklet_run(entries, COUNT(entries), name, 0));
    assert(!mulib_web_worklet_run(entries + 3, 1, name, 0));
    assert(!mulib_web_worklet_run(entries, COUNT(entries), (muba_span){NULL, 0}, 0));
    assert(!mulib_web_worklet_run(entries, COUNT(entries), (muba_span){NULL, 3}, 0));
    assert(!mulib_web_worklet_run(entries, COUNT(entries), muba_str("absent"), 0));
    assert(!mulib_web_worklet_run(NULL, 0, name, 0));
    assert(a.calls == 2 && !b.calls);
}

static void locks(void)
{
    muba_span name = muba_str("lock");
    mulib_web_locks_held held[] = {{name, 1, 0}, {muba_str("other"), 2, 1}};
    assert(mulib_web_locks_available(NULL, 0, name, 1));
    assert(!mulib_web_locks_available(NULL, 1, name, 1));
    assert(!mulib_web_locks_available(NULL, 0, (muba_span){NULL, 1}, 0));
    assert(mulib_web_locks_available(held, COUNT(held), name, 0));
    assert(!mulib_web_locks_available(held, COUNT(held), name, 1));
    assert(mulib_web_locks_available(held, COUNT(held), muba_str("new"), 1));
    held[0].exclusive = 1;
    assert(!mulib_web_locks_available(held, COUNT(held), name, 0));
    held[1].name = (muba_span){NULL, 1};
    assert(!mulib_web_locks_available(held, COUNT(held), muba_str("new"), 0));
    held[1].name = (muba_span){NULL, 0};
    assert(!mulib_web_locks_available(held, COUNT(held), held[1].name, 0));
}

typedef struct {
    mulib_web_taskqueue_t *scheduler;
    unsigned *trace;
    unsigned label;
} task_context;

static void task(void *opaque)
{
    task_context *ctx = opaque;
    *ctx->trace = 10 * *ctx->trace + ctx->label;
    assert(!mulib_web_taskqueue_step(ctx->scheduler));
}

static void task_sources(void)
{
    muba_job jobs[3][2];
    muba_queue sources[COUNT(jobs)];
    mulib_web_taskqueue_t scheduler = {sources, COUNT(sources), 0, 0};
    unsigned trace = 0;
    task_context a = {&scheduler, &trace, 1}, b = {&scheduler, &trace, 2};
    for (size_t i = 0; i < COUNT(sources); i++)
        assert(muba_queue_init(&sources[i], jobs[i], COUNT(jobs[i])));
    assert(!mulib_web_taskqueue_step(&scheduler));
    assert(muba_queue_push(&sources[0], task, &a));
    assert(muba_queue_push(&sources[0], task, &a));
    assert(muba_queue_push(&sources[2], task, &b));
    assert(mulib_web_taskqueue_step(&scheduler) && trace == 1);
    assert(mulib_web_taskqueue_step(&scheduler) && trace == 12);
    assert(mulib_web_taskqueue_step(&scheduler) && trace == 121);
    assert(!mulib_web_taskqueue_step(&scheduler));
}

static void identities(void)
{
    mulib_web_storagekey_t a = {muba_str("origin"), muba_str("site"), 2};
    mulib_web_storagekey_t b = a;
    muba_span url = muba_str("https://example.test/worker"), name = muba_str("x");
    assert(mulib_web_sharedworker_match(a, url, name, b, url, name));
    assert(!mulib_web_sharedworker_match(a, url, name, b, url, muba_str("X")));
    b.site = muba_str("other");
    assert(!mulib_web_sharedworker_match(a, url, name, b, url, name));
    b = a;
    b.nonce++;
    assert(!mulib_web_sharedworker_match(a, url, name, b, url, name));

    const mulib_web_workerclient_entry clients[] = {{1, 2, 0, 0}, {1, 3, 0, 1}, {1, 2, 0, 1}};
    assert(mulib_web_workerclient_find(clients, COUNT(clients), 1, 2) == &clients[2]);
    assert(!mulib_web_workerclient_find(clients, COUNT(clients), 1, 1));
    mulib_web_workerglobal_t global = {10, 20, 3, 0};
    assert(mulib_web_workerglobal_exposed(&global, 6));
    assert(!mulib_web_workerglobal_exposed(&global, 4));
    global.closing = 1;
    assert(!mulib_web_workerglobal_exposed(&global, 1));
    assert(mulib_web_agentcluster_shared(1, 1, 1, 1));
    assert(!mulib_web_agentcluster_shared(0, 0, 1, 1));
    assert(!mulib_web_agentcluster_shared(1, 2, 1, 1));
    assert(!mulib_web_agentcluster_shared(1, 1, 0, 1));
    assert(!mulib_web_agentcluster_shared(1, 1, 1, 0));
}

int main(void)
{
    lifecycle();
    scopes();
    dispatch();
    locks();
    task_sources();
    identities();
    return 0;
}
