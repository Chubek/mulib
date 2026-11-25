#define _POSIX_C_SOURCE 200809L
#include "on1x_kernel_resolve.h"

#include "on1x_kernel_package.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Use the canonical CDD implementation without exporting its ordinary symbols
 * from the runtime archive (which may coexist with a caller's CDD library). */
#define mulib_cdd_name_valid on1x_kernel_cdd_name_valid
#define mulib_cdd_value_valid on1x_kernel_cdd_value_valid
#define mulib_cdd_req_valid on1x_kernel_cdd_req_valid
#define mulib_cdd_cap_matches on1x_kernel_cdd_cap_matches
#define mulib_cdd_entry_provides on1x_kernel_cdd_entry_provides
#define mulib_cdd_entries_conflict on1x_kernel_cdd_entries_conflict
#define mulib_cdd_attr_get on1x_kernel_cdd_attr_get
#define mulib_cdd_find on1x_kernel_cdd_find
#define mulib_cdd_validate on1x_kernel_cdd_validate
#define mulib_cdd_problem_valid on1x_kernel_cdd_problem_valid
#define mulib_cdd_satisfied on1x_kernel_cdd_satisfied
#define mulib_cdd_can_append on1x_kernel_cdd_can_append
#define mulib_cdd_plan_validate on1x_kernel_cdd_plan_validate
#define MULIB_IMPL_CDD_CATALOGUE
#define MULIB_IMPL_CDD_PLAN
#include <mulib-cdd-plan.h>

#define MAX_ROOT_LIST_BYTES 4096u
#define MAX_PACKAGE_BYTES_TOTAL (32u * 1024u * 1024u)
#define NO_NODE ((size_t)-1)

_Static_assert(ON1X_KERNEL_RESOLVE_MAX_NAME == ON1X_KERNEL_PACKAGE_NAME_MAX,
               "resolver and package name limits must agree");
_Static_assert(ON1X_KERNEL_RESOLVE_MAX_PACKAGES <= MULIB_CDD_MAX_ENTRIES,
               "resolution must fit a CDD plan");

enum visit_state { VISITING, FINISHED };

struct node {
    struct on1x_kernel_package package;
    unsigned char *bytes;
    size_t bytes_size;
    size_t parent;
    const char *origin_export; /* Used only during resolution. */
    enum visit_state state;
    int goal;
};

struct on1x_kernel_resolution {
    struct node nodes[ON1X_KERNEL_RESOLVE_MAX_PACKAGES];
    size_t order[ON1X_KERNEL_RESOLVE_MAX_PACKAGES];
    size_t count, order_count;
};

struct resolve_context {
    struct on1x_kernel_resolution *resolution;
    struct on1x_kernel_resolve_diag *diag;
    int roots[ON1X_KERNEL_RESOLVE_MAX_ROOTS];
    size_t root_count, bytes_read;
};

static size_t find_node(const struct on1x_kernel_resolution *resolution,
                        const char *module)
{
    if (resolution && module) {
        for (size_t i = 0; i < resolution->count; ++i) {
            if (strcmp(module, resolution->nodes[i].package.module) == 0)
                return i;
        }
    }
    return NO_NODE;
}

static void write_chain(const struct on1x_kernel_resolution *resolution,
                        size_t parent, const char *tail,
                        char out[ON1X_KERNEL_RESOLVE_CHAIN_SIZE])
{
    size_t ancestors[ON1X_KERNEL_RESOLVE_MAX_PACKAGES], n = 0;
    for (size_t i = parent; i != NO_NODE; i = resolution->nodes[i].parent)
        ancestors[n++] = i;
    char *p = out;
    while (n) {
        const char *name = resolution->nodes[ancestors[--n]].package.module;
        size_t length = strlen(name);
        memcpy(p, name, length);
        p += length;
        memcpy(p, " -> ", 4);
        p += 4;
    }
    /* Names here are either parser-validated, import-validated, or "<invalid>". */
    strcpy(p, tail);
}

size_t on1x_kernel_resolution_count(const struct on1x_kernel_resolution *resolution)
{
    return resolution ? resolution->order_count : 0;
}

const struct on1x_kernel_package *on1x_kernel_resolution_at(
    const struct on1x_kernel_resolution *resolution, size_t ordered_index)
{
    if (!resolution || ordered_index >= resolution->order_count) return NULL;
    return &resolution->nodes[resolution->order[ordered_index]].package;
}

const struct on1x_kernel_package *on1x_kernel_resolution_find(
    const struct on1x_kernel_resolution *resolution, const char *module)
{
    size_t i = find_node(resolution, module);
    return i == NO_NODE ? NULL : &resolution->nodes[i].package;
}

const void *on1x_kernel_resolution_bytes(
    const struct on1x_kernel_resolution *resolution, size_t ordered_index, size_t *size)
{
    if (size) *size = 0;
    if (!resolution || ordered_index >= resolution->order_count) return NULL;
    const struct node *node = &resolution->nodes[resolution->order[ordered_index]];
    if (size) *size = node->bytes_size;
    return node->bytes;
}

int on1x_kernel_resolution_chain(const struct on1x_kernel_resolution *resolution,
                                  const char *module,
                                  char chain[ON1X_KERNEL_RESOLVE_CHAIN_SIZE])
{
    if (!chain) return 0;
    chain[0] = '\0';
    size_t i = find_node(resolution, module);
    if (i == NO_NODE) return 0;
    const struct node *node = &resolution->nodes[i];
    write_chain(resolution, node->parent, node->package.module, chain);
    return 1;
}

void on1x_kernel_resolution_free(struct on1x_kernel_resolution *resolution)
{
    if (!resolution) return;
    for (size_t i = resolution->count; i != 0; --i)
        free(resolution->nodes[i - 1].bytes);
    free(resolution);
}

static int fail(struct resolve_context *ctx, size_t parent, const char *module,
                const char *export_name, const char *reason)
{
    if (ctx->diag) {
        struct on1x_kernel_resolve_diag *diag = ctx->diag;
        snprintf(diag->module, sizeof(diag->module), "%s", module);
        snprintf(diag->export_name, sizeof(diag->export_name), "%s", export_name);
        snprintf(diag->reason, sizeof(diag->reason), "%s", reason);
        write_chain(ctx->resolution, parent, module, diag->chain);
    }
    return -1;
}

/* For rejected records, keep printable export labels without echoing arbitrary
 * input in the process diagnostic. Valid records use the parser's exact rules. */
static const char *safe_export_label(const char *name)
{
    if (!name || !*name) return "<invalid>";
    for (size_t i = 0; i <= ON1X_KERNEL_RESOLVE_MAX_NAME; ++i) {
        char c = name[i];
        if (c == '\0') return name;
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'))
            break;
    }
    return "<invalid>";
}

/* Open roots once, so each module is looked up in the same directory objects.
 * Root paths are host configuration; the untrusted leaf lookup never follows a
 * symlink and a missing leaf is the only condition that allows another root. */
static const char *open_roots(struct resolve_context *ctx, const char *list)
{
    if (!list || !*list) return "no explicit package roots";
    size_t length = strnlen(list, MAX_ROOT_LIST_BYTES + 1u);
    if (length > MAX_ROOT_LIST_BYTES) return "package root list exceeds byte limit";
    char paths[MAX_ROOT_LIST_BYTES + 1u];
    memcpy(paths, list, length + 1u);
    for (char *path = paths;;) {
        char *next = strchr(path, ':');
        if (next) *next++ = '\0';
        if (!*path) return "empty package root";
        if (ctx->root_count == ON1X_KERNEL_RESOLVE_MAX_ROOTS)
            return "package root count exceeds limit";
        int fd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (fd < 0) return "cannot open package root directory";
        ctx->roots[ctx->root_count++] = fd;
        if (!next) return NULL;
        path = next;
    }
}

static ssize_t read_some(int fd, void *bytes, size_t length)
{
    ssize_t n;
    do { n = read(fd, bytes, length); } while (n < 0 && errno == EINTR);
    return n;
}

static const char *read_package(struct resolve_context *ctx, const char *module,
                                struct node *node)
{
    char filename[ON1X_KERNEL_PACKAGE_NAME_MAX + sizeof(ON1X_KERNEL_PACKAGE_SUFFIX)];
    snprintf(filename, sizeof(filename), "%s%s", module, ON1X_KERNEL_PACKAGE_SUFFIX);
    int fd = -1;
    struct stat status;
    const char *reason = NULL;
    for (size_t i = 0; i < ctx->root_count; ++i) {
        struct stat candidate;
        if (fstatat(ctx->roots[i], filename, &candidate, AT_SYMLINK_NOFOLLOW) != 0) {
            if (errno == ENOENT) continue;
            reason = "cannot inspect package candidate";
            goto done;
        }
        if (!S_ISREG(candidate.st_mode)) {
            reason = "package candidate is not a regular non-symlink file";
            goto done;
        }
        if (fd >= 0) {
            reason = "ambiguous package: found in more than one root";
            goto done;
        }
        /* Recheck the opened file: another process might replace the leaf
         * between fstatat and openat. NONBLOCK avoids waiting on a raced FIFO. */
        fd = openat(ctx->roots[i], filename,
                    O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK | O_NOCTTY);
        if (fd < 0 || fstat(fd, &status) != 0 || !S_ISREG(status.st_mode) ||
            status.st_dev != candidate.st_dev || status.st_ino != candidate.st_ino) {
            reason = "cannot open unchanged regular package candidate";
            goto done;
        }
    }
    if (fd < 0) return "package not found in explicit roots";
    if (status.st_size <= 0) {
        reason = "malformed package: empty file";
        goto done;
    }
    if ((uintmax_t)status.st_size > ON1X_KERNEL_PACKAGE_SIZE_MAX) {
        reason = "package file exceeds byte limit";
        goto done;
    }
    size_t size = (size_t)status.st_size;
    if (size > MAX_PACKAGE_BYTES_TOTAL - ctx->bytes_read) {
        reason = "total package bytes exceed limit";
        goto done;
    }
    node->bytes = malloc(size);
    if (!node->bytes) {
        reason = "out of memory reading package";
        goto done;
    }
    ctx->bytes_read += size;
    node->bytes_size = size;
    size_t offset = 0;
    while (offset < size) {
        ssize_t n = read_some(fd, node->bytes + offset, size - offset);
        if (n <= 0) {
            reason = "cannot read complete package file";
            goto done;
        }
        offset += (size_t)n;
    }
    unsigned char extra;
    if (read_some(fd, &extra, 1) != 0) {
        reason = "package file changed or is unreadable";
        goto done;
    }
    if (!on1x_kernel_package_parse(node->bytes, size, &node->package)) {
        reason = "malformed package or unsupported metadata";
        goto done;
    }
    if (strcmp(module, node->package.module) != 0)
        reason = "package module does not match filename";
done:
    if (fd >= 0) (void)close(fd);
    return reason;
}

static int visit(struct resolve_context *ctx, const char *module, uint32_t version,
                 size_t parent, const char *origin_export, size_t *index)
{
    struct on1x_kernel_resolution *r = ctx->resolution;
    size_t i = find_node(r, module);
    int known = i != NO_NODE;
    if (!known) {
        if (r->count == ON1X_KERNEL_RESOLVE_MAX_PACKAGES)
            return fail(ctx, parent, module, origin_export, "package graph exceeds node limit");
        i = r->count++;
        const char *reason = read_package(ctx, module, &r->nodes[i]);
        if (reason) return fail(ctx, parent, module, origin_export, reason);
        r->nodes[i].parent = parent;
        r->nodes[i].origin_export = origin_export;
    }
    struct node *node = &r->nodes[i];
    if (node->package.version != version) {
        char reason[128];
        snprintf(reason, sizeof(reason), "%s version mismatch: required %" PRIu32
                 ", found %" PRIu32, parent == NO_NODE ? "package" : "dependency",
                 version, node->package.version);
        return fail(ctx, parent, module, origin_export, reason);
    }
    if (known) {
        if (node->state == VISITING)
            return fail(ctx, parent, module, origin_export, "dependency cycle");
        *index = i;
        return 0;
    }
    node->state = VISITING;
    for (size_t j = 0; j < node->package.dep_count; ++j) {
        const struct on1x_kernel_package_dep *dep = &node->package.deps[j];
        size_t dependency;
        if (visit(ctx, dep->module, dep->version, i, origin_export, &dependency) != 0)
            return -1;
    }
    node->state = FINISHED;
    r->order[r->order_count++] = i;
    *index = i;
    return 0;
}

static int match_export(struct resolve_context *ctx, size_t index,
                        const struct on1x_kernel_import *required)
{
    const struct on1x_kernel_package *package = &ctx->resolution->nodes[index].package;
    const char *reason = "required export not found in package";
    for (size_t j = 0; j < package->export_count; ++j) {
        const struct on1x_kernel_package_export *actual = &package->exports[j];
        if (strcmp(required->export_name, actual->name) != 0) continue;
        if (required->arity != actual->arity)
            reason = "export arity mismatch";
        else if (strcmp(required->interface_sha256, actual->interface_sha256) != 0)
            reason = "interface digest mismatch";
        else if (strcmp(required->behavior_sha256, actual->behavior_sha256) != 0)
            reason = "behavior digest mismatch";
        else
            return 0;
        break;
    }
    return fail(ctx, NO_NODE, required->module, required->export_name, reason);
}

/* CDD independently checks a plan of the exact reachable closure. The explicit
 * discovery walk gives dependency witnesses (including cycles); there is no
 * provider search or cost optimization because module/version is exact and
 * multiple providers of the same module were rejected during discovery. */
static int validate_cdd(struct resolve_context *ctx)
{
    struct on1x_kernel_resolution *r = ctx->resolution;
    if (r->order_count != r->count) {
        const struct node *node = &r->nodes[0];
        return fail(ctx, NO_NODE, node->package.module, node->origin_export,
                    "incomplete package dependency plan");
    }
    mulib_cdd_entry_t entries[ON1X_KERNEL_RESOLVE_MAX_PACKAGES] = {0};
    mulib_cdd_cap_t provided[ON1X_KERNEL_RESOLVE_MAX_PACKAGES];
    mulib_cdd_req_t deps[ON1X_KERNEL_RESOLVE_MAX_PACKAGES][ON1X_KERNEL_PACKAGE_DEP_MAX];
    const char *conflicts[ON1X_KERNEL_RESOLVE_MAX_PACKAGES][ON1X_KERNEL_PACKAGE_CONFLICT_MAX];
    mulib_cdd_req_t goals[ON1X_KERNEL_RESOLVE_MAX_PACKAGES];
    size_t goal_count = 0;
    mulib_cdd_plan_t plan = {.count = r->order_count, .cost = r->count, .found = 1};
    for (size_t i = 0; i < r->count; ++i) {
        const struct on1x_kernel_package *package = &r->nodes[i].package;
        provided[i] = (mulib_cdd_cap_t){package->module, package->version};
        if (r->nodes[i].goal) {
            goals[goal_count++] =
                (mulib_cdd_req_t){package->module, package->version, package->version};
        }
        for (size_t j = 0; j < package->dep_count; ++j) {
            const struct on1x_kernel_package_dep *dep = &package->deps[j];
            deps[i][j] = (mulib_cdd_req_t){dep->module, dep->version, dep->version};
        }
        for (size_t j = 0; j < package->conflict_count; ++j)
            conflicts[i][j] = package->conflicts[j].module;
        entries[i] = (mulib_cdd_entry_t){
            .id = package->module, .category = "on1x.kernel", .implementation = "package",
            .revision = package->version, .cost = 1u, .provides = &provided[i],
            .provide_count = 1u, .requirements = deps[i], .require_count = package->dep_count,
            .conflicts = conflicts[i], .conflict_count = package->conflict_count
        };
        plan.order[i] = r->order[i];
    }
    const mulib_cdd_catalogue_t catalogue = {.entries = entries, .count = r->count};
    const mulib_cdd_problem_t problem = {
        .goals = goals, .goal_count = goal_count, .allowed = UINT64_MAX, .max_cost = r->count
    };
    size_t failed = MULIB_CDD_NONE;
    mulib_cdd_status_t status = mulib_cdd_plan_validate(&catalogue, &problem, &plan, &failed);
    if (status == MULIB_CDD_OK) return 0;
    size_t i = plan.order[failed < plan.count ? failed : 0];
    const struct node *node = &r->nodes[i];
    if (status == MULIB_CDD_CONFLICT && failed < plan.count) {
        for (size_t j = 0; j < failed; ++j) {
            size_t other = plan.order[j];
            if (mulib_cdd_entries_conflict(&entries[i], &entries[other])) {
                char reason[128];
                snprintf(reason, sizeof(reason), "package conflict with %s", entries[other].id);
                return fail(ctx, node->parent, node->package.module, node->origin_export, reason);
            }
        }
    }
    return fail(ctx, node->parent, node->package.module, node->origin_export,
                "CDD rejected the package dependency plan");
}

int on1x_kernel_resolve(const char *root_list,
                        const struct on1x_kernel_import *required, size_t count,
                        struct on1x_kernel_resolution **out,
                        struct on1x_kernel_resolve_diag *diag)
{
    struct resolve_context ctx = {.diag = diag};
    if (diag) memset(diag, 0, sizeof(*diag));
    if (!out) return fail(&ctx, NO_NODE, "<unknown>", "<unknown>", "no resolution output");
    *out = NULL;
    if (count == 0) return 0;
    if (!required || count > ON1X_KERNEL_RESOLVE_MAX_IMPORTS)
        return fail(&ctx, NO_NODE, "<unknown>", "<unknown>", "invalid kernel import count");
    for (size_t i = 0; i < count; ++i) {
        const struct on1x_kernel_import *import = &required[i];
        if (!on1x_kernel_import_valid(import)) {
            const char *module = on1x_kernel_package_module_valid(import->module)
                                 ? import->module : "<invalid>";
            return fail(&ctx, NO_NODE, module, safe_export_label(import->export_name),
                        "invalid kernel import record");
        }
    }
    const char *root_error = open_roots(&ctx, root_list);
    int result = -1;
    if (root_error) {
        fail(&ctx, NO_NODE, required[0].module, required[0].export_name, root_error);
        goto done;
    }
    ctx.resolution = calloc(1, sizeof(*ctx.resolution));
    if (!ctx.resolution) {
        fail(&ctx, NO_NODE, required[0].module, required[0].export_name,
             "out of memory building package graph");
        goto done;
    }
    for (size_t i = 0; i < count; ++i) {
        const struct on1x_kernel_import *import = &required[i];
        size_t index;
        if (visit(&ctx, import->module, import->version, NO_NODE, import->export_name, &index) != 0 ||
            match_export(&ctx, index, import) != 0)
            goto done;
        ctx.resolution->nodes[index].goal = 1;
    }
    if (validate_cdd(&ctx) != 0) goto done;
    for (size_t i = 0; i < ctx.resolution->count; ++i)
        ctx.resolution->nodes[i].origin_export = NULL;
    *out = ctx.resolution;
    ctx.resolution = NULL;
    result = 0;
done:
    while (ctx.root_count) (void)close(ctx.roots[--ctx.root_count]);
    on1x_kernel_resolution_free(ctx.resolution);
    return result;
}
