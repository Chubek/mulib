#define _POSIX_C_SOURCE 200809L
#include "../stdppx/luaembed/runtime.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__real_realloc(void *, size_t);
static size_t calls, fail_at;
static int tracking;

static int denied(void)
{
    if (!tracking) return 0;
    ++calls;
    return fail_at && calls == fail_at;
}
void *__wrap_malloc(size_t n) { return denied() ? NULL : __real_malloc(n); }
void *__wrap_calloc(size_t n, size_t s) { return denied() ? NULL : __real_calloc(n,s); }
void *__wrap_realloc(void *p, size_t n) { return denied() ? NULL : __real_realloc(p,n); }

static bool evaluate(void)
{
    return on1x_luaembed_eval("local L=require('llembed'); L.emit(L.integer(42))",
                             "(Loc \"oom.on1x\" 0 1 1)", "LuaEmbed", "(Unit 1 ())");
}
int main(void)
{
    size_t total, sample;
    tracking = 1;
    assert(evaluate());
    total = calls;
    tracking = 0;
    assert(total > 128);
    /* Deterministically sample the entire parse/state/library/execution/cleanup
     * allocation lifetime, not only its first few allocations. */
    for (sample = 1; sample <= 128; ++sample) {
        pid_t child = fork();
        int status;
        assert(child >= 0);
        if (!child) {
            bool ok;
            calls = 0; fail_at = 1 + (total - 1) * (sample - 1) / 127;
            tracking = 1;
            ok = evaluate();
            tracking = 0;
            if (ok) {
                assert(!strcmp(on1x_luaembed_output(),
                    "(DecimalLiteral \"42\" (Loc \"oom.on1x\" 0 1 1))\n"));
            } else {
                assert(!*on1x_luaembed_output());
                assert(*on1x_luaembed_error());
            }
            exit(0);
        }
        assert(waitpid(child, &status, 0) == child);
        assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    return 0;
}
