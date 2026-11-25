#include "on1x_kernel_eval.h"
#include "on1x_kernel_recipe.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "kernel evaluator check failed at line %d: %s\n", \
                __LINE__, #condition); \
        exit(1); \
    } \
} while (0)

/* Checked-in copy of the reviewed clamp expressions, independent of
 * any build-generated recipe or native implementation. */
#define CLAMP_HEADER "(ffispec 1 (kernel clamp) (header mulib-clamp.h)" \
    " (sha256 cf3d0cf6e27b0d2844e25ea1e5bf7637a34508d29c42411aa855dfe97a1205e0)"

static const char clamp_recipe[] =
    CLAMP_HEADER
    " (export i64 mulib_clamp_i64 (params x lo hi)"
    "  (guard (le lo hi))"
    "  (body (if (lt x lo) lo (if (gt x hi) hi x))))"
    " (export add_i64 mulib_clamp_add_i64 (params a b)"
    "  (guard true)"
    "  (body (if (and (gt b 0) (gt a (sub 9223372036854775807 b)))"
    "   9223372036854775807"
    "   (if (and (lt b 0) (lt a (sub -9223372036854775808 b)))"
    "    -9223372036854775808 (add a b)))))"
    " (export sub_i64 mulib_clamp_sub_i64 (params a b)"
    "  (guard true)"
    "  (body (if (and (lt b 0) (gt a (add 9223372036854775807 b)))"
    "   9223372036854775807"
    "   (if (and (gt b 0) (lt a (add -9223372036854775808 b)))"
    "    -9223372036854775808 (sub a b))))))";

/* Parser-verified, but deliberately not approved package behavior. These
 * expressions exercise overflow paths not reachable in reviewed clamp. */
static const char control_recipe[] =
    CLAMP_HEADER
    " (export i64 mulib_clamp_i64 (params x lo hi)"
    "  (guard (and (ge x lo) (or (eq x hi) (lt (add x 1) hi))))"
    "  (body (if (eq x hi) 42 (sub x 1)))))";

static const char branch_recipe[] =
    CLAMP_HEADER
    " (export i64 mulib_clamp_i64 (params x lo hi)"
    "  (guard true)"
    "  (body (if (eq lo hi) (add x 1) (sub x 1)))))";

static void parse(const char *source, struct on1x_kernel_recipe *recipe) {
    struct on1x_kernel_recipe_error error = {0};
    if (!on1x_kernel_recipe_parse(source, strlen(source), recipe, &error)) {
        fprintf(stderr, "recipe rejected at %zu: %s\n", error.offset, error.message);
        exit(1);
    }
}

static const struct on1x_kernel_verified_export *
member(const struct on1x_kernel_recipe *recipe, const char *name) {
    for (size_t i = 0; i < recipe->export_count; ++i)
        if (strcmp(recipe->exports[i].member, name) == 0)
            return &recipe->exports[i];
    fprintf(stderr, "recipe missing member %s\n", name);
    exit(1);
}

static void expect(const struct on1x_kernel_verified_export *recipe, size_t arity,
                   int64_t a, int64_t b, int64_t c, int status, int64_t result) {
    int64_t args[] = {a, b, c};
    const int64_t untouched = -789;
    int64_t out = untouched;
    int got = on1x_kernel_eval(recipe, args, arity, &out);
    if (got != status || out != (status == ON1X_KERNEL_EVAL_OK ? result : untouched)) {
        fprintf(stderr, "%s(%" PRId64 ", %" PRId64 ", %" PRId64
                "): status %d, out %" PRId64 " (expected status %d, out %" PRId64 ")\n",
                recipe->member, a, b, c, got, out, status,
                status == ON1X_KERNEL_EVAL_OK ? result : untouched);
        exit(1);
    }
}

int main(void) {
    struct on1x_kernel_recipe *recipe = malloc(sizeof(*recipe));
    CHECK(recipe);
    parse(clamp_recipe, recipe);
    CHECK(recipe->export_count == 3);

    const struct on1x_kernel_verified_export *clamp = member(recipe, "i64");
    expect(clamp, 3, 27, 0, 10, ON1X_KERNEL_EVAL_OK, 10);
    expect(clamp, 3, INT64_MIN, -10, 0, ON1X_KERNEL_EVAL_OK, -10);
    expect(clamp, 3, 4, 0, 10, ON1X_KERNEL_EVAL_OK, 4);
    expect(clamp, 3, INT64_MAX, INT64_MIN, INT64_MIN, ON1X_KERNEL_EVAL_OK, INT64_MIN);
    expect(clamp, 3, 0, 10, -10, ON1X_KERNEL_EVAL_GUARD, 0);
    expect(clamp, 2, 27, 0, 10, ON1X_KERNEL_EVAL_INVALID, 0);

    const struct on1x_kernel_verified_export *add = member(recipe, "add_i64");
    expect(add, 2, INT64_MAX, 1, 0, ON1X_KERNEL_EVAL_OK, INT64_MAX);
    expect(add, 2, INT64_MIN, -1, 0, ON1X_KERNEL_EVAL_OK, INT64_MIN);
    expect(add, 2, INT64_MAX, INT64_MIN, 0, ON1X_KERNEL_EVAL_OK, -1);
    expect(add, 2, INT64_MIN, INT64_MAX, 0, ON1X_KERNEL_EVAL_OK, -1);
    expect(add, 2, INT64_MAX - 1, 1, 0, ON1X_KERNEL_EVAL_OK, INT64_MAX);
    expect(add, 2, INT64_MIN + 1, -1, 0, ON1X_KERNEL_EVAL_OK, INT64_MIN);

    const struct on1x_kernel_verified_export *sub = member(recipe, "sub_i64");
    expect(sub, 2, INT64_MAX, -1, 0, ON1X_KERNEL_EVAL_OK, INT64_MAX);
    expect(sub, 2, INT64_MIN, 1, 0, ON1X_KERNEL_EVAL_OK, INT64_MIN);
    expect(sub, 2, -1, INT64_MIN, 0, ON1X_KERNEL_EVAL_OK, INT64_MAX);
    expect(sub, 2, INT64_MIN, INT64_MIN, 0, ON1X_KERNEL_EVAL_OK, 0);
    expect(sub, 2, INT64_MAX - 1, -1, 0, ON1X_KERNEL_EVAL_OK, INT64_MAX);
    expect(sub, 2, INT64_MIN + 1, 1, 0, ON1X_KERNEL_EVAL_OK, INT64_MIN);

    parse(control_recipe, recipe);
    clamp = member(recipe, "i64");
    expect(clamp, 3, INT64_MIN, 0, 1, ON1X_KERNEL_EVAL_GUARD, 0);
    expect(clamp, 3, INT64_MAX, 0, INT64_MAX, ON1X_KERNEL_EVAL_OK, 42);
    expect(clamp, 3, INT64_MAX, 0, 1, ON1X_KERNEL_EVAL_OVERFLOW, 0);
    expect(clamp, 3, INT64_MIN, INT64_MIN, 0, ON1X_KERNEL_EVAL_OVERFLOW, 0);

    parse(branch_recipe, recipe);
    clamp = member(recipe, "i64");
    expect(clamp, 3, INT64_MIN, 0, 0, ON1X_KERNEL_EVAL_OK, INT64_MIN + 1);
    expect(clamp, 3, INT64_MAX, 0, 1, ON1X_KERNEL_EVAL_OK, INT64_MAX - 1);
    expect(clamp, 3, INT64_MAX, 0, 0, ON1X_KERNEL_EVAL_OVERFLOW, 0);
    expect(clamp, 3, INT64_MIN, 0, 1, ON1X_KERNEL_EVAL_OVERFLOW, 0);
    free(recipe);
    return 0;
}
