#include "on1x_kernel_recipe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do {                                                        \
    if (!(expr)) {                                                              \
        fprintf(stderr, "kernel recipe check failed at line %d: %s\n",            \
                __LINE__, #expr);                                               \
        exit(1);                                                                \
    }                                                                           \
} while (0)

#define SOURCE_SHA "cf3d0cf6e27b0d2844e25ea1e5bf7637a34508d29c42411aa855dfe97a1205e0"
#define METADATA " (kernel clamp) (header mulib-clamp.h) (sha256 " SOURCE_SHA ")"
#define DOC "(ffispec 1" METADATA
#define EXPORT " (export i64 mulib_clamp_i64 (params a b)"

static const char canonical[] =
    DOC EXPORT " (guard (le a b)) (body (if (lt a b) -9223372036854775808 (sub 0 1)))))";
static const char presentation[] =
    "(ffispec 1\r\n"
    " (export i64 mulib_clamp_i64 (target *) (body (if (lt lhs rhs)"
    " -9223372036854775808\t(sub -0;inline comment\n 1)))"
    " (hint kernel-hot (target x86_64-linux-gnu) (speculative))"
    " (rewrite fold-identity) (params lhs rhs) (guard (le lhs rhs)))"
    " (backend mir-c11) (header mulib-clamp.h) (kernel clamp)"
    " (sha256 0000000000000000000000000000000000000000000000000000000000000000))"
    " # end of recipe\n";
static const char changed[] =
    DOC EXPORT " (guard (le a b)) (body (if (lt a b) -9223372036854775808 (sub 1 0)))))";
/* Hash independently computed from (guard (le $0 $1))\n(body (if (lt $0 $1)
 * -9223372036854775808 (sub 0 1)))\n, on exactly two lines. */
static const char digest[] =
    "4fa350e2aaaac9e66c78cadc4a24c3e36788531f1026928f5857c5da7bfd3eea";

static void good(const char *input, size_t n, struct on1x_kernel_recipe *recipe)
{
    struct on1x_kernel_recipe_error error;
    if (!on1x_kernel_recipe_parse(input, n, recipe, &error)) {
        fprintf(stderr, "unexpected recipe rejection at %zu: %s\n", error.offset, error.message);
        exit(1);
    }
    CHECK(!error.message);
}

static void bad(const char *input, size_t n, struct on1x_kernel_recipe *recipe)
{
    struct on1x_kernel_recipe_error error;
    CHECK(!on1x_kernel_recipe_parse(input, n, recipe, &error));
    CHECK(error.message && error.offset <= n);
}

static size_t binary_tree(char *buf, unsigned leaves)
{
    if (leaves == 1) {
        *buf = 'a';
        return 1;
    }
    size_t n = 5;
    memcpy(buf, "(add ", n);
    n += binary_tree(buf + n, leaves / 2);
    buf[n++] = ' ';
    n += binary_tree(buf + n, leaves - leaves / 2);
    buf[n++] = ')';
    return n;
}

static void limits(struct on1x_kernel_recipe *recipe)
{
    char *buf = malloc(ON1X_KERNEL_RECIPE_MAX_BYTES + 1u);
    CHECK(buf);

    const size_t simple_size = sizeof canonical - 1u;
    memcpy(buf, canonical, simple_size);
    memset(buf + simple_size, ' ', ON1X_KERNEL_RECIPE_MAX_BYTES + 1u - simple_size);
    good(buf, ON1X_KERNEL_RECIPE_MAX_BYTES, recipe);
    bad(buf, ON1X_KERNEL_RECIPE_MAX_BYTES + 1u, recipe);
    buf[simple_size] = '#';
    buf[simple_size + 1] = '\0'; /* NUL is forbidden even inside a comment. */
    bad(buf, simple_size + 2, recipe);

    /* Every proper prefix excludes a required token or closing delimiter. */
    for (size_t n = 0; n < simple_size; ++n)
        bad(canonical, n, recipe);

    size_t n = sizeof DOC - 1u;
    memcpy(buf, DOC, n);
    for (unsigned i = 0; i <= ON1X_KERNEL_RECIPE_MAX_EXPORTS; ++i) {
        const int len = sprintf(buf + n, " (export e%u s%u (params a b) (guard true) (body a))",
                                i, i);
        CHECK(len > 0);
        n += (size_t)len;
        buf[n] = ')';
        if (i + 1u == ON1X_KERNEL_RECIPE_MAX_EXPORTS)
            good(buf, n + 1, recipe);
    }
    bad(buf, n + 1, recipe);

    static const char prefix[] = DOC EXPORT " (guard true) (body ";
    const size_t start = sizeof prefix - 1u;
    memcpy(buf, prefix, start);
    for (unsigned depth = ON1X_KERNEL_RECIPE_MAX_DEPTH;
         depth <= ON1X_KERNEL_RECIPE_MAX_DEPTH + 1u; ++depth) {
        n = start;
        for (unsigned i = 0; i < depth; ++i) {
            memcpy(buf + n, "(add 0 ", 7);
            n += 7;
        }
        buf[n++] = 'a';
        for (unsigned i = 0; i < depth + 3u; ++i)
            buf[n++] = ')';
        if (depth == ON1X_KERNEL_RECIPE_MAX_DEPTH)
            good(buf, n, recipe);
        else
            bad(buf, n, recipe);
    }
    for (unsigned leaves = ON1X_KERNEL_RECIPE_MAX_EXPR_NODES / 2u;
         leaves <= ON1X_KERNEL_RECIPE_MAX_EXPR_NODES / 2u + 1u; ++leaves) {
        n = start + binary_tree(buf + start, leaves);
        memcpy(buf + n, ")))", 3);
        if (2 * leaves - 1u <= ON1X_KERNEL_RECIPE_MAX_EXPR_NODES)
            good(buf, n + 3, recipe);
        else
            bad(buf, n + 3, recipe);
    }
    free(buf);
}

int main(void)
{
    struct on1x_kernel_recipe *recipe = malloc(sizeof *recipe);
    CHECK(recipe);

    char hex[65];
    CHECK(on1x_kernel_sha256("abc", 3, hex));
    CHECK(strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);
    good(canonical, sizeof canonical - 1u, recipe);
    CHECK(strcmp(recipe->kernel, "clamp") == 0);
    CHECK(strcmp(recipe->header, "mulib-clamp.h") == 0);
    CHECK(strcmp(recipe->source_sha256, SOURCE_SHA) == 0);
    CHECK(recipe->export_count == 1);
    const struct on1x_kernel_verified_export *exp = &recipe->exports[0];
    CHECK(strcmp(exp->member, "i64") == 0 && strcmp(exp->symbol, "mulib_clamp_i64") == 0);
    CHECK(exp->arity == 2 && exp->guard == 2 && exp->body == 10 && exp->node_count == 11);
    CHECK(exp->nodes[exp->guard].op == ON1X_KERNEL_OP_LE);
    CHECK(exp->nodes[exp->guard].a == 0 && exp->nodes[exp->guard].b == 1);
    CHECK(exp->nodes[exp->body].op == ON1X_KERNEL_OP_IF);
    CHECK(exp->nodes[exp->nodes[exp->body].b].immediate == INT64_MIN);
    CHECK(strcmp(exp->behavior_sha256, digest) == 0);

    char scratch[sizeof presentation];
    memcpy(scratch, presentation, sizeof scratch);
    good(scratch, sizeof scratch - 1u, recipe);
    memset(scratch, 0, sizeof scratch);
    CHECK(strcmp(exp->behavior_sha256, digest) == 0);
    CHECK(strcmp(recipe->source_sha256, SOURCE_SHA) != 0);

    good(changed, sizeof changed - 1u, recipe);
    /* A repeated source SHA claim cannot authorize a different typed body. */
    CHECK(strcmp(recipe->source_sha256, SOURCE_SHA) == 0);
    CHECK(strcmp(exp->behavior_sha256, digest) != 0);

    const char *invalid[] = {
        "(ffi-catalogue 1" METADATA ")",
        "(ffispec 2" METADATA ")",
        DOC ")",
        DOC EXPORT " (guard true) (body a))) (extra)",
        DOC EXPORT " (guard true) (guard true) (body a)))",
        DOC EXPORT " (guard true) (body a b)))",
        DOC EXPORT " (guard true) (body (add a)))",
        DOC EXPORT " (guard true) (body (mul a b))))",
        DOC EXPORT " (guard a) (body b)))",
        DOC EXPORT " (guard (eq true true)) (body b)))",
        DOC EXPORT " (guard true) (body (add (if true a b) 0))))",
        DOC EXPORT " (guard true) (body 00)))",
        DOC EXPORT " (guard true) (body +1)))",
        DOC EXPORT " (guard true) (body 9223372036854775808)))",
        DOC EXPORT " (guard true) (body -9223372036854775809)))",
        DOC EXPORT " (guard true) (body $0)))",
        DOC EXPORT " (guard true) (body a) (hint eval)))",
        DOC EXPORT " (guard true) (body a) (hint kernel-hot (target *) (target *))))",
        DOC " (export e s (params a a) (guard true) (body a)))",
        DOC " (export e s (params true b) (guard true) (body b)))",
        DOC " (export e s (params a) (guard true) (body a)))",
        DOC " (export e s (params a b c d) (guard true) (body a)))",
        DOC " (export e s (params a b) (guard true) (body a))"
            " (export e t (params a b) (guard true) (body a)))",
        DOC " (export e s (params a b) (guard true) (body a))"
            " (export f s (params a b) (guard true) (body a)))",
        "(ffispec 1 (kernel clamp) (header mulib-other.h) (sha256 " SOURCE_SHA ")"
            EXPORT " (guard true) (body a)))",
    };
    for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; ++i)
        bad(invalid[i], strlen(invalid[i]), recipe);
    limits(recipe);
    free(recipe);
    return 0;
}
