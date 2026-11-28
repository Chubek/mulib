#include "mulib-browser-diff.h"
#include <assert.h>

typedef struct {
    unsigned kind[8];
    uint64_t id[8];
    size_t calls, reject_at;
} diff_log;

static int
record(void *ctx, unsigned kind, uint64_t id)
{
    diff_log *log = ctx;
    assert(log->calls < sizeof log->id / sizeof log->id[0]);
    log->kind[log->calls] = kind;
    log->id[log->calls++] = id;
    return log->calls != log->reject_at;
}

int
main(void)
{
    const mulib_browser_diff_entry old[] = {{7, 3}, {1, 0}, {4, 2}};
    const mulib_browser_diff_entry now[] = {{9, 0}, {4, 2}, {7, 4}, {2, 8}};
    const mulib_browser_diff_entry repeated[] = {{3, 0}, {4, 2}, {3, 1}};
    const mulib_browser_diff_entry zero[] = {{9, 0}, {0, 1}};
    diff_log log = {0};

    assert(!mulib_browser_diff_run(repeated, 3, now, 4, record, &log));
    assert(!mulib_browser_diff_run(old, 3, repeated, 3, record, &log));
    assert(!mulib_browser_diff_run(old, 3, zero, 2, record, &log));
    assert(!mulib_browser_diff_run(zero, 2, NULL, 0, record, &log));
    assert(!mulib_browser_diff_run(NULL, 0, repeated, 3, record, &log));
    assert(!mulib_browser_diff_run(NULL, 1, now, 4, record, &log));
    assert(!mulib_browser_diff_run(old, 3, NULL, 1, record, &log));
    assert(mulib_browser_diff_run(NULL, 0, NULL, 0, record, &log));
    assert(mulib_browser_diff_run(old, 3, old, 3, record, &log));
    assert(log.calls == 0);

    assert(mulib_browser_diff_run(old, 3, now, 4, record, &log));
    assert(log.calls == 4);
    assert(log.kind[0] == 2 && log.id[0] == 7);
    assert(log.kind[1] == 3 && log.id[1] == 1);
    assert(log.kind[2] == 1 && log.id[2] == 9);
    assert(log.kind[3] == 1 && log.id[3] == 2);

    log.calls = 0;
    log.reject_at = 2;
    assert(!mulib_browser_diff_run(old, 3, now, 4, record, &log));
    assert(log.calls == 2 && log.kind[1] == 3 && log.id[1] == 1);
    return 0;
}
