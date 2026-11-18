/* Trusted supervisor's broker: plugin identity/grants never come from IPC. */
#define MULIB_IMPL_SANDBOX_PROFILE
#define MULIB_IMPL_CAPBROKER
#include "mulib-capbroker.h"
#include <stdio.h>
#include <string.h>
static int echo(void *ctx, const char *subject, const void *request,
                size_t n, void *reply, size_t cap, size_t *len) {
    (void)ctx; (void)subject;
    if (n > cap) return MULIB_SB_LIMIT;
    if (n) memcpy(reply, request, n);
    *len = n;
    return 0;
}
int main(void) {
    const char *requested[] = {"echo", "ledger.write"};
    const char *approved[] = {"echo"};
    const char *grants[2]; size_t count;
    mulib_capbroker_op_t ops[] = {{"echo", "echo", echo, NULL}};
    mulib_capbroker_t broker;
    mulib_sandbox_profile_t profile = mulib_sandbox_profile_default();
    if (mulib_capbroker_intersect(requested, 2, approved, 1, grants, 2, &count) ||
        mulib_capbroker_init(&broker, "example", ops, 1, grants, count, NULL, &profile) ||
        mulib_capbroker_begin(&broker)) return 1;
    char reply[32]; size_t n;
    int rc = mulib_capbroker_call(&broker, "echo", "hello", 5, reply, sizeof reply, &n);
    int result = mulib_capbroker_finish(&broker, rc == 0);
    if (rc || result) return 2;
    printf("%.*s\n", (int)n, reply);
    return 0;
}
