#include "on1x_runtime.h"
#include <assert.h>
#include <string.h>
struct capture { on1x_env header; int value; };
static int trace, destroyed;
static void invoke(on1x_env *env) {
    struct capture *c = (struct capture *)env;
    trace = trace * 10 + c->value;
}
static void destroy(on1x_env *env) {
    struct capture *c = (struct capture *)env;
    destroyed += c->value;
}
static void push(on1x_defer_stack *stack, int n) {
    struct capture *c = (struct capture *)on1x_defer_register(
        stack, sizeof(*c), invoke, destroy, "runtime-test", 42, 7);
    c->value = n;
}
static void nested(on1x_env *env) {
    on1x_defer_stack stack = {0};
    assert(!env);
    push(&stack, 3); on1x_defer_run(&stack);
    assert(!stack.head && !stack.count && !stack.bytes);
}
int main(int argc, char **argv) {
    on1x_defer_stack a = {0}, b = {0};
    if (argc > 1) {
        size_t size = strcmp(argv[1], "overflow") == 0 ? SIZE_MAX : 1;
        (void)on1x_defer_register(&a, size, invoke, destroy, "runtime-test", 42, 7);
        return 1;
    }
    push(&a, 1); push(&b, 9); push(&a, 2);
    assert(a.count == 2 && b.count == 1 && a.bytes == b.bytes * 2);
    on1x_defer_run(&b); assert(trace == 9 && destroyed == 9);
    on1x_defer_run(&a); assert(trace == 921 && destroyed == 12);
    assert(!a.head && !a.count && !a.bytes && !b.head && !b.count && !b.bytes);
    on1x_defer_run(&a); assert(trace == 921);
    (void)on1x_defer_register(&a, 0, nested, NULL, "runtime-test", 42, 7);
    on1x_defer_run(&a); assert(trace == 9213 && destroyed == 15);
    assert(!a.head && !a.count && !a.bytes);
    return 0;
}
