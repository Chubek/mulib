/* Offline synchronous lifecycle example; no script or thread is created. */
#include "mulib-web-worker.h"
#include <assert.h>

static int start(void *ctx)
{
    int *running = ctx;
    *running = 1;
    return 1;
}

static void stop(void *ctx)
{
    int *running = ctx;
    *running = 0;
}

int main(void)
{
    int running = 0;
    mulib_web_worker_t worker = {mulib_web_worker_NEW, start, stop, &running, 0};
    mulib_web_worker_t canceled = worker;
    assert(mulib_web_worker_terminate(&canceled));
    assert(!mulib_web_worker_start(&canceled) && !running);
    assert(mulib_web_worker_start(&worker) && running);
    assert(mulib_web_worker_terminate(&worker) && !running);
    assert(!mulib_web_worker_terminate(&worker));
    return 0;
}
