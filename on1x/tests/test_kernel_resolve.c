#define _POSIX_C_SOURCE 200809L
#include "on1x_kernel_package.h"
#include "on1x_kernel_resolve.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "kernel resolver test failed at line %d: %s\n", __LINE__, #condition); \
        exit(1); \
    } \
} while (0)

/* The resolver is tested on structurally valid synthetic package manifests.
 * The loader, not the resolver, checks recipe behavior and admits code. */
static const unsigned char recipe[] = "synthetic resolver-only fixture";
static const char behavior[] =
    "1111111111111111111111111111111111111111111111111111111111111111";
static const char other_digest[] =
    "2222222222222222222222222222222222222222222222222222222222222222";

static void path_for(char path[256], const char *root, const char *module)
{
    int n = snprintf(path, 256, "%s/%s%s", root, module, ON1X_KERNEL_PACKAGE_SUFFIX);
    CHECK(n > 0 && n < 256);
}

static struct on1x_kernel_package package(const char *module)
{
    struct on1x_kernel_package out = {
        .kind = ON1X_KERNEL_PACKAGE_RECIPE, .version = 1, .export_count = 1,
        .recipe = recipe, .recipe_size = sizeof(recipe) - 1u
    };
    strcpy(out.module, module);
    struct on1x_kernel_package_export *e = &out.exports[0];
    strcpy(e->name, "add_i64");
    e->arity = 2;
    CHECK(on1x_kernel_hash_interface(out.module, e->name, e->arity, e->interface_sha256));
    strcpy(e->behavior_sha256, behavior);
    return out;
}

static void dep(struct on1x_kernel_package *p, const char *module, uint32_t version)
{
    CHECK(p->dep_count < ON1X_KERNEL_PACKAGE_DEP_MAX);
    struct on1x_kernel_package_dep *d = &p->deps[p->dep_count++];
    strcpy(d->module, module);
    d->version = version;
}

static void conflict(struct on1x_kernel_package *p, const char *module)
{
    CHECK(p->conflict_count < ON1X_KERNEL_PACKAGE_CONFLICT_MAX);
    strcpy(p->conflicts[p->conflict_count++].module, module);
}

static void publish(const char *root, const char *as_module,
                    const struct on1x_kernel_package *p)
{
    size_t size = 0, written = 0;
    CHECK(on1x_kernel_package_encode(p, NULL, 0, &size));
    unsigned char *bytes = malloc(size);
    CHECK(bytes);
    CHECK(on1x_kernel_package_encode(p, bytes, size, &written));
    CHECK(written == size);
    char path[256];
    path_for(path, root, as_module);
    FILE *out = fopen(path, "wb");
    CHECK(out);
    CHECK(fwrite(bytes, 1, size, out) == size);
    CHECK(fclose(out) == 0);
    free(bytes);
}

static void unpublish(const char *root, const char *module)
{
    char path[256];
    path_for(path, root, module);
    CHECK(unlink(path) == 0);
}

static struct on1x_kernel_import import_for(const struct on1x_kernel_package *p)
{
    const struct on1x_kernel_package_export *e = &p->exports[0];
    return (struct on1x_kernel_import){
        .module = p->module, .export_name = e->name, .arity = e->arity,
        .version = p->version, .interface_sha256 = e->interface_sha256,
        .behavior_sha256 = e->behavior_sha256
    };
}

static void expect_failure(const char *roots, const struct on1x_kernel_import *imports,
                           size_t count, const char *module, const char *reason,
                           const char *chain)
{
    struct on1x_kernel_resolution *resolution = NULL;
    struct on1x_kernel_resolve_diag diag = {0};
    CHECK(on1x_kernel_resolve(roots, imports, count, &resolution, &diag) != 0);
    CHECK(resolution == NULL);
    if (strcmp(diag.module, module) || !strstr(diag.reason, reason) || strcmp(diag.chain, chain)) {
        fprintf(stderr, "unexpected diagnostic: %s/%s: %s [%s]\n",
                diag.module, diag.export_name, diag.reason, diag.chain);
        exit(1);
    }
    CHECK(strcmp(diag.export_name, imports[0].export_name) == 0);
}

static void discovery(const char *root1, const char *root2, const char *roots)
{
    struct on1x_kernel_package a = package("kern.a");
    struct on1x_kernel_package b = package("kern.b");
    struct on1x_kernel_import import = import_for(&a);
    publish(root1, a.module, &a);
    expect_failure(NULL, &import, 1, a.module, "no explicit package roots", a.module);
    expect_failure(":", &import, 1, a.module, "empty package root", a.module);
    char invalid_list[300];
    CHECK(snprintf(invalid_list, sizeof(invalid_list), "%s:", root1) > 0);
    expect_failure(invalid_list, &import, 1, a.module, "empty package root", a.module);
    CHECK(snprintf(invalid_list, sizeof(invalid_list), "%s:%s/nope", root1, root2) > 0);
    expect_failure(invalid_list, &import, 1, a.module, "cannot open package root", a.module);
    char long_list[4098];
    memset(long_list, 'a', sizeof(long_list) - 1u);
    long_list[sizeof(long_list) - 1u] = '\0';
    expect_failure(long_list, &import, 1, a.module, "root list exceeds byte limit", a.module);
    const char repeated[] = ".:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.";
    expect_failure(repeated, &import, 1, a.module, "root count exceeds limit", a.module);
    expect_failure(root2, &import, 1, a.module, "package not found", a.module);

    publish(root2, a.module, &a);
    expect_failure(roots, &import, 1, a.module, "ambiguous package", a.module);
    unpublish(root2, a.module);
    publish(root2, a.module, &b);
    expect_failure(root2, &import, 1, a.module, "module does not match filename", a.module);
    unpublish(root2, a.module);

    char path[256], target[256];
    path_for(path, root2, a.module);
    path_for(target, "../root1", a.module);
    CHECK(symlink(target, path) == 0);
    expect_failure(roots, &import, 1, a.module, "not a regular non-symlink", a.module);
    unpublish(root2, a.module);
    CHECK(mkfifo(path, 0600) == 0);
    expect_failure(root2, &import, 1, a.module, "not a regular non-symlink", a.module);
    unpublish(root2, a.module);

    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    CHECK(fd >= 0);
    CHECK(write(fd, "bad", 3) == 3);
    expect_failure(root2, &import, 1, a.module, "malformed package", a.module);
    CHECK(ftruncate(fd, ON1X_KERNEL_PACKAGE_SIZE_MAX + 1u) == 0);
    expect_failure(root2, &import, 1, a.module, "file exceeds byte limit", a.module);
    CHECK(close(fd) == 0);
    unpublish(root2, a.module);

    struct on1x_kernel_import wrong = import;
    wrong.version = 2;
    expect_failure(roots, &wrong, 1, a.module, "package version mismatch", a.module);
    wrong = import;
    wrong.arity = 3;
    expect_failure(roots, &wrong, 1, a.module, "arity mismatch", a.module);
    wrong = import;
    wrong.export_name = "unknown_i64";
    expect_failure(roots, &wrong, 1, a.module, "required export not found", a.module);
    wrong = import;
    wrong.interface_sha256 = other_digest;
    expect_failure(roots, &wrong, 1, a.module, "interface digest mismatch", a.module);
    wrong = import;
    wrong.behavior_sha256 = other_digest;
    expect_failure(roots, &wrong, 1, a.module, "behavior digest mismatch", a.module);
    wrong.behavior_sha256 = "not a digest";
    expect_failure(roots, &wrong, 1, a.module, "invalid kernel import record", a.module);
    wrong = import;
    wrong.module = "../kern.a";
    expect_failure(roots, &wrong, 1, "<invalid>", "invalid kernel import record", "<invalid>");
    unpublish(root1, a.module);
}

static void dependency_graph(const char *root1, const char *root2, const char *roots)
{
    struct on1x_kernel_package a = package("kern.a");
    struct on1x_kernel_package b = package("kern.b");
    struct on1x_kernel_package c = package("kern.c");
    struct on1x_kernel_package d = package("kern.d");
    struct on1x_kernel_import imports[] = {import_for(&a), import_for(&c)};
    dep(&a, b.module, 1);
    dep(&a, c.module, 1);
    dep(&b, d.module, 1);
    dep(&c, d.module, 1);
    publish(root1, a.module, &a);
    publish(root1, b.module, &b);
    publish(root2, c.module, &c);
    expect_failure(roots, imports, 2, d.module, "package not found",
                   "kern.a -> kern.b -> kern.d");
    d.version = 2;
    publish(root2, d.module, &d);
    expect_failure(roots, imports, 2, d.module, "dependency version mismatch",
                   "kern.a -> kern.b -> kern.d");
    d.version = 1;
    dep(&d, a.module, 1);
    publish(root2, d.module, &d);
    expect_failure(roots, imports, 2, a.module, "dependency cycle",
                   "kern.a -> kern.b -> kern.d -> kern.a");
    d.dep_count = 0;
    publish(root2, d.module, &d);
    /* One-sided conflict declared by the already-selected entry, to exercise
     * CDD's symmetric conflict test. */
    conflict(&b, c.module);
    publish(root1, b.module, &b);
    expect_failure(roots, imports, 2, c.module, "package conflict with kern.b",
                   "kern.a -> kern.c");
    b.conflict_count = 0;
    /* An unselected conflict target does not cause a lookup or admission. */
    conflict(&b, "kern.unselected");
    publish(root1, b.module, &b);

    struct on1x_kernel_resolution *r = NULL;
    struct on1x_kernel_resolve_diag diag;
    CHECK(on1x_kernel_resolve(roots, imports, 2, &r, &diag) == 0);
    CHECK(r && !diag.reason[0] && on1x_kernel_resolution_count(r) == 4);
    const char *expected[] = {d.module, b.module, c.module, a.module};
    for (size_t i = 0; i < 4; ++i) {
        const struct on1x_kernel_package *p = on1x_kernel_resolution_at(r, i);
        CHECK(p && strcmp(p->module, expected[i]) == 0);
        CHECK(p == on1x_kernel_resolution_find(r, expected[i]));
    }
    char chain[ON1X_KERNEL_RESOLVE_CHAIN_SIZE];
    CHECK(on1x_kernel_resolution_chain(r, d.module, chain));
    CHECK(strcmp(chain, "kern.a -> kern.b -> kern.d") == 0);
    CHECK(!on1x_kernel_resolution_chain(r, "kern.unselected", chain));
    CHECK(!chain[0] && !on1x_kernel_resolution_at(r, 4));
    size_t length = 1;
    CHECK(!on1x_kernel_resolution_bytes(r, 4, &length) && length == 0);
    unpublish(root1, a.module);
    unpublish(root1, b.module);
    unpublish(root2, c.module);
    unpublish(root2, d.module);
    const void *bytes = on1x_kernel_resolution_bytes(r, 0, &length);
    CHECK(bytes && length && on1x_kernel_package_parse(bytes, length, &d));
    CHECK(d.recipe_size == sizeof(recipe) - 1u && !memcmp(d.recipe, recipe, d.recipe_size));
    on1x_kernel_resolution_free(r);
}

static void numbered_module(char name[64], size_t number)
{
    memset(name, 'a', 63);
    name[1] = '.';
    CHECK(snprintf(name + 60, 4, "%03zu", number) == 3);
}

static void graph_bounds(const char *root)
{
    char module[64], next[64], expected[ON1X_KERNEL_RESOLVE_CHAIN_SIZE] = {0};
    char *out = expected;
    for (size_t i = 0; i <= ON1X_KERNEL_RESOLVE_MAX_PACKAGES; ++i) {
        numbered_module(module, i);
        if (i) {
            memcpy(out, " -> ", 4);
            out += 4;
        }
        memcpy(out, module, 63);
        out += 63;
        if (i != ON1X_KERNEL_RESOLVE_MAX_PACKAGES) {
            struct on1x_kernel_package p = package(module);
            numbered_module(next, i + 1u);
            dep(&p, next, 1);
            publish(root, p.module, &p);
        }
    }
    numbered_module(next, 0);
    struct on1x_kernel_package first = package(next);
    struct on1x_kernel_import import = import_for(&first);
    expect_failure(root, &import, 1, module, "graph exceeds node limit", expected);
    numbered_module(module, ON1X_KERNEL_RESOLVE_MAX_PACKAGES - 1u);
    struct on1x_kernel_package last = package(module);
    publish(root, module, &last);
    struct on1x_kernel_resolution *r = NULL;
    CHECK(on1x_kernel_resolve(root, &import, 1, &r, NULL) == 0);
    CHECK(on1x_kernel_resolution_count(r) == ON1X_KERNEL_RESOLVE_MAX_PACKAGES);
    CHECK(strcmp(on1x_kernel_resolution_at(r, 0)->module, module) == 0);
    on1x_kernel_resolution_free(r);
    for (size_t i = 0; i < ON1X_KERNEL_RESOLVE_MAX_PACKAGES; ++i) {
        numbered_module(module, i);
        unpublish(root, module);
    }
}

int main(void)
{
    struct on1x_kernel_resolution *r = NULL;
    CHECK(on1x_kernel_resolve(NULL, NULL, 0, &r, NULL) == 0 && !r);
    struct on1x_kernel_package first = package("kern.first");
    struct on1x_kernel_import import = import_for(&first);
    struct on1x_kernel_resolve_diag diag;
    CHECK(on1x_kernel_resolve(NULL, &import, ON1X_KERNEL_RESOLVE_MAX_IMPORTS + 1u,
                             &r, &diag) != 0 && !r);
    CHECK(strcmp(diag.reason, "invalid kernel import count") == 0);
    char temp[] = "./on1x-kernel-resolve-XXXXXX";
    CHECK(mkdtemp(temp));
    char root1[64], root2[64], roots[130];
    CHECK(snprintf(root1, sizeof(root1), "%s/root1", temp) > 0);
    CHECK(snprintf(root2, sizeof(root2), "%s/root2", temp) > 0);
    CHECK(mkdir(root1, 0700) == 0 && mkdir(root2, 0700) == 0);
    CHECK(snprintf(roots, sizeof(roots), "%s:%s", root1, root2) > 0);
    discovery(root1, root2, roots);
    dependency_graph(root1, root2, roots);
    graph_bounds(root1);
    CHECK(rmdir(root1) == 0 && rmdir(root2) == 0 && rmdir(temp) == 0);
    return 0;
}
