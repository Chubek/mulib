/* Isolated tests for the internal, single-threaded closure environment ABI. */
#include "on1x_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct chain {
    on1x_env header;
    on1x_env *child;
    size_t position;
};
static size_t destroyed, expected;

static void destroy(on1x_env *environment) {
    struct chain *value = (struct chain *)environment;
    if (value->position != expected || !expected) abort();
    --expected;
    ++destroyed;
    on1x_env_release(value->child);
}

int main(int argc, char **argv) {
    on1x_env *environment = NULL;
    size_t i;
    on1x_env_retain(NULL);
    on1x_env_release(NULL);
    if (argc > 1 && strcmp(argv[1], "invalid-size") == 0) {
        (void)on1x_env_alloc(sizeof(on1x_env) - 1, NULL);
        abort();
    }
    if (argc > 1 && strcmp(argv[1], "overflow") == 0) {
        on1x_env value = {SIZE_MAX, NULL, NULL};
        on1x_env_retain(&value);
        abort();
    }
    if (argc > 1 && strcmp(argv[1], "underflow") == 0) {
        on1x_env value = {0, NULL, NULL};
        on1x_env_release(&value);
        abort();
    }
    if (argc > 1 && strcmp(argv[1], "invalid-retain") == 0) {
        on1x_env value = {0, NULL, NULL};
        on1x_env_retain(&value);
        abort();
    }
    environment = on1x_env_alloc(sizeof(on1x_env), NULL);
    if (environment->references != 1 || environment->next || environment->destroy) abort();
    on1x_env_retain(environment);
    if (environment->references != 2) abort();
    on1x_env_release(environment);
    if (environment->references != 1) abort();
    on1x_env_release(environment);
    environment = NULL;
    /* Transferred child references form a chain without recursive C calls. */
    for (i = 1; i <= 100000; ++i) {
        struct chain *value = (struct chain *)on1x_env_alloc(sizeof(struct chain), destroy);
        value->position = i;
        value->child = environment;
        environment = &value->header;
    }
    expected = 100000;
    on1x_env_release(environment);
    if (destroyed != 100000 || expected) abort();
    /* Draining must reset, so a later independent release works too. */
    on1x_env_release(on1x_env_alloc(sizeof(on1x_env), NULL));
    puts("Closure runtime: reference counts and iterative destruction PASS");
    return 0;
}
