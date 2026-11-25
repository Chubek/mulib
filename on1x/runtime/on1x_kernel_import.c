#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "on1x_kernel_import.h"
#include "on1x_kernel_eval.h"
#include "on1x_kernel_native.h"
#include "on1x_kernel_package.h"
#include "on1x_kernel_recipe.h"
#include "on1x_kernel_resolve.h"

#include <errno.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct import_slot {
    struct on1x_kernel_import identity;
    char module[ON1X_KERNEL_PACKAGE_NAME_MAX + 1u];
    struct on1x_kernel_package_export export_info;
    const struct on1x_kernel_verified_export *recipe;
    struct on1x_kernel_native_adapter native;
};

struct import_owner {
    char module[ON1X_KERNEL_PACKAGE_NAME_MAX + 1u];
    char package_sha256[65];
    struct on1x_kernel_recipe *checked;
    struct on1x_kernel_native *native;
};

struct import_table {
    size_t owner_count;
    size_t count;
    struct import_owner owners[ON1X_KERNEL_RESOLVE_MAX_PACKAGES];
    struct import_slot slots[];
};

static _Atomic(struct import_table *) published;

static int report(const char *module, const char *export_name, const char *reason) {
    fprintf(stderr, "on1x: kernel import %s/%s: %s\n",
            module && *module ? module : "<unknown>",
            export_name && *export_name ? export_name : "<unknown>",
            reason);
    return -1;
}

static int report_import(const struct on1x_kernel_import *identity, const char *reason) {
    return report(identity ? identity->module : NULL,
                  identity ? identity->export_name : NULL, reason);
}

static int report_chain(const char *module, const char *export_name,
                        const char *reason, const char *chain) {
    if (chain && *chain)
        fprintf(stderr, "on1x: kernel import %s/%s: %s (dependency chain: %s)\n",
                module && *module ? module : "<unknown>",
                export_name && *export_name ? export_name : "<unknown>", reason, chain);
    else
        return report(module, export_name, reason);
    return -1;
}

static int report_package(const struct on1x_kernel_resolution *resolution,
                          const struct on1x_kernel_package *package,
                          const char *export_name, const char *reason) {
    char chain[ON1X_KERNEL_RESOLVE_CHAIN_SIZE] = {0};
    (void)on1x_kernel_resolution_chain(resolution, package->module, chain);
    return report_chain(package->module, export_name, reason, chain);
}

static const char *identity_difference(const struct on1x_kernel_import *actual,
                                       const struct on1x_kernel_import *expected) {
    if (strcmp(actual->module, expected->module)) return "module mismatch";
    if (strcmp(actual->export_name, expected->export_name)) return "export mismatch";
    if (actual->version != expected->version) return "kernel ABI version mismatch";
    if (actual->arity != expected->arity) return "export arity mismatch";
    if (strcmp(actual->interface_sha256, expected->interface_sha256))
        return "interface SHA-256 mismatch";
    if (strcmp(actual->behavior_sha256, expected->behavior_sha256))
        return "behavior SHA-256 mismatch";
    return NULL;
}

static int match_published(const struct import_table *table,
                           const struct on1x_kernel_import *required, size_t count) {
    if (table->count != count) {
        const struct on1x_kernel_import *different = table->count < count
            ? &required[table->count] : &table->slots[count].identity;
        return report_import(different, "incompatible published import table size");
    }
    for (size_t i = 0; i < count; ++i) {
        const char *reason = identity_difference(&table->slots[i].identity, &required[i]);
        if (reason) return report_import(&required[i], reason);
    }
    return 0;
}

#ifdef ON1X_KERNEL_IMPORT_TESTING
static int fail_slot(size_t slot) {
    const char *value = getenv("ON1X_KERNEL_IMPORT_FAIL_SLOT");
    if (!value || !*value) return 0;
    size_t number = 0;
    for (const char *p = value; *p; ++p) {
        if (*p < '0' || *p > '9') return 0;
        unsigned digit = (unsigned)(*p - '0');
        if (number > (SIZE_MAX - digit) / 10) return 0;
        number = number * 10 + digit;
    }
    return number == slot;
}

static void trace(const char *event, size_t index, const char *module, const char *export_name) {
    const char *enabled = getenv("ON1X_KERNEL_IMPORT_TRACE");
    if (enabled && strcmp(enabled, "1") == 0)
        fprintf(stderr, "on1x: kernel import trace %s %zu %s/%s\n",
                event, index, module, export_name);
}
#else
#define fail_slot(slot) 0
#define trace(event, index, module, export_name) ((void)0)
#endif

static const struct on1x_kernel_verified_export *find_recipe(
    const struct on1x_kernel_recipe *checked, const char *member) {
    for (size_t i = 0; i < checked->export_count; ++i)
        if (strcmp(checked->exports[i].member, member) == 0) return &checked->exports[i];
    return NULL;
}

static const struct on1x_kernel_package_export *find_export(
    const struct on1x_kernel_package *package, const char *name) {
    for (size_t i = 0; i < package->export_count; ++i)
        if (strcmp(package->exports[i].name, name) == 0) return &package->exports[i];
    return NULL;
}

static const char *named_export(const struct on1x_kernel_package *package,
                                const struct on1x_kernel_import *required, size_t count) {
    for (size_t i = 0; i < count; ++i)
        if (strcmp(required[i].module, package->module) == 0) return required[i].export_name;
    return package->export_count ? package->exports[0].name : "<package>";
}

static const char *verify_recipe(const struct on1x_kernel_package *package,
                                 struct on1x_kernel_recipe *checked,
                                 const char **export_name, char reason[256]) {
    struct on1x_kernel_recipe_error error = {0};
    if (!on1x_kernel_recipe_parse(package->recipe, package->recipe_size, checked, &error)) {
        snprintf(reason, 256, "invalid checked recipe at byte %zu: %s",
                 error.offset, error.message ? error.message : "parse failed");
        return reason;
    }
    static const char prefix[] = "stdffi.";
    if (strncmp(package->module, prefix, sizeof prefix - 1u) != 0 ||
        strcmp(package->module + sizeof prefix - 1u, checked->kernel) != 0)
        return "checked recipe module mismatch";
    /* Resolution has checked each application's separate pin, and structural
     * decoding recomputed interface digests. Validate the executable recipe
     * against every manifest export, including unrequested exports. */
    for (size_t i = 0; i < package->export_count; ++i) {
        const struct on1x_kernel_package_export *export_info = &package->exports[i];
        const struct on1x_kernel_verified_export *recipe = find_recipe(checked, export_info->name);
        *export_name = export_info->name;
        if (!recipe) return "export missing from checked recipe";
        if (recipe->arity != export_info->arity) return "checked recipe export arity mismatch";
        if (strcmp(recipe->behavior_sha256, export_info->behavior_sha256) != 0)
            return "checked recipe behavior SHA-256 mismatch";
    }
    if (checked->export_count != package->export_count) {
        for (size_t i = 0; i < checked->export_count; ++i)
            if (!find_export(package, checked->exports[i].member)) {
                *export_name = checked->exports[i].member;
                break;
            }
        return "checked recipe export set differs from package";
    }
    return NULL;
}

static ssize_t read_policy_chunk(int fd, void *buffer, size_t size) {
    ssize_t n;
    do {
        n = read(fd, buffer, size);
    } while (n < 0 && errno == EINTR);
    return n;
}

static const char *read_native_policy(unsigned char policy[ON1X_KERNEL_NATIVE_POLICY_LIMIT],
                                      size_t *size) {
    const char *path = getenv("ON1X_KERNEL_NATIVE_POLICY");
    if (!path || !*path) return "native package requires explicit ON1X_KERNEL_NATIVE_POLICY";
    int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) return "cannot open explicit native policy file";
    struct stat st;
    const char *reason = "native policy must be a nonempty bounded regular file";
    if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode) &&
        st.st_size > 0 && (uintmax_t)st.st_size <= ON1X_KERNEL_NATIVE_POLICY_LIMIT) {
        size_t end = (size_t)st.st_size;
        size_t offset = 0;
        while (offset < end) {
            ssize_t n = read_policy_chunk(fd, policy + offset, end - offset);
            if (n <= 0) break;
            offset += (size_t)n;
        }
        unsigned char extra;
        reason = "native policy changed size or could not be read";
        if (offset == end && read_policy_chunk(fd, &extra, 1) == 0) {
            *size = end;
            reason = NULL;
        }
    }
    (void)close(fd);
    return reason;
}

static const char *open_native(const struct on1x_kernel_package *package,
                               struct on1x_kernel_native **out) {
    if (strcmp(package->module, "stdffi.clamp") != 0 ||
        package->version != ON1X_KERNEL_IMPORT_VERSION)
        return "native package is not supported for this module and version";
    unsigned char policy[ON1X_KERNEL_NATIVE_POLICY_LIMIT];
    size_t size = 0;
    const char *reason = read_native_policy(policy, &size);
    if (reason) return reason;
    struct on1x_kernel_native_binding bindings[ON1X_KERNEL_PACKAGE_EXPORT_MAX];
    for (size_t i = 0; i < package->export_count; ++i) {
        const struct on1x_kernel_package_export *e = &package->exports[i];
        bindings[i] = (struct on1x_kernel_native_binding){
            e->name, e->arity, e->interface_sha256, e->behavior_sha256
        };
    }
    if (on1x_kernel_native_open(package->image, package->image_size, policy, size,
                                bindings, package->export_count, out, &reason) == 0)
        return NULL;
    return reason ? reason : "native package admission failed";
}

static int stage_owners(struct import_table *table,
                        const struct on1x_kernel_resolution *resolution,
                        const struct on1x_kernel_import *required, size_t count) {
    size_t packages = on1x_kernel_resolution_count(resolution);
    for (size_t i = 0; i < packages; ++i) {
        const struct on1x_kernel_package *package = on1x_kernel_resolution_at(resolution, i);
        const char *name = named_export(package, required, count);
        struct import_owner *owner = &table->owners[i];
        strcpy(owner->module, package->module);
        size_t size = 0;
        const void *bytes = on1x_kernel_resolution_bytes(resolution, i, &size);
        if (!bytes || !on1x_kernel_sha256(bytes, size, owner->package_sha256))
            return report_package(resolution, package, name, "cannot hash selected package");
        owner->checked = malloc(sizeof(*owner->checked));
        if (!owner->checked)
            return report_package(resolution, package, name, "out of memory checking recipe");
        ++table->owner_count; /* Record ownership before checking anything else. */
        trace("acquire-recipe", i, owner->module, "-");
        char detail[256];
        const char *reason = verify_recipe(package, owner->checked, &name, detail);
        if (reason) return report_package(resolution, package, name, reason);
        if (package->kind == ON1X_KERNEL_PACKAGE_NATIVE_AMD64_LINUX_V1) {
            name = named_export(package, required, count);
            reason = open_native(package, &owner->native);
            if (reason) return report_package(resolution, package, name, reason);
            trace("acquire-native", i, owner->module, "-");
        }
    }
    return 0;
}

static int stage_slot(struct import_table *table, size_t index,
                      const struct on1x_kernel_resolution *resolution,
                      const struct on1x_kernel_import *required) {
    struct import_owner *owner = NULL;
    for (size_t i = 0; i < table->owner_count; ++i)
        if (strcmp(table->owners[i].module, required->module) == 0) {
            owner = &table->owners[i];
            break;
        }
    const struct on1x_kernel_package *package =
        on1x_kernel_resolution_find(resolution, required->module);
    if (!owner || !package) return report_import(required, "no staged module owner");
    const struct on1x_kernel_package_export *export_info =
        find_export(package, required->export_name);
    if (!export_info) return report_import(required, "no staged export");

    struct import_slot *slot = &table->slots[index];
    strcpy(slot->module, owner->module);
    slot->export_info = *export_info;
    slot->identity = (struct on1x_kernel_import){
        slot->module, slot->export_info.name, package->version, slot->export_info.arity,
        slot->export_info.interface_sha256, slot->export_info.behavior_sha256
    };
    const char *reason = identity_difference(&slot->identity, required);
    if (reason) return report_import(required, reason);
    slot->recipe = find_recipe(owner->checked, required->export_name);
    if (!slot->recipe) return report_import(required, "no staged checked export");
    if (owner->native) {
        if (on1x_kernel_native_get(owner->native, export_info->name, export_info->arity,
                                   &slot->native) != 0 ||
            (export_info->arity == 2 && (!slot->native.call2 || slot->native.call3)) ||
            (export_info->arity == 3 && (!slot->native.call3 || slot->native.call2)))
            return report_import(required, "cannot bind typed native adapter");
    }
    ++table->count;
    trace("bind-slot", index, slot->identity.module, slot->identity.export_name);
    if (fail_slot(index)) return report_import(required, "injected post-bind failure");
    return 0;
}

static void rollback(struct import_table *table) {
    /* Bindings borrow their owners. Drop all dispatch references before
     * releasing owner resources in the reverse of dependency-first staging. */
    while (table->count) {
        size_t i = --table->count;
        struct import_slot *slot = &table->slots[i];
        slot->recipe = NULL;
        slot->native = (struct on1x_kernel_native_adapter){0};
        trace("unbind-slot", i, slot->identity.module, slot->identity.export_name);
    }
    while (table->owner_count) {
        size_t i = --table->owner_count;
        struct import_owner *owner = &table->owners[i];
        if (owner->native) {
            on1x_kernel_native_close(owner->native);
            trace("release-native", i, owner->module, "-");
        }
        free(owner->checked);
        trace("release-recipe", i, owner->module, "-");
    }
    free(table);
}

static void trace_publication(const struct import_table *table) {
    const char *enabled = getenv("ON1X_KERNEL_TRACE");
    if (enabled && strcmp(enabled, "1") == 0)
        for (size_t i = 0; i < table->owner_count; ++i) {
            const struct import_owner *owner = &table->owners[i];
            fprintf(stderr, "on1x: kernel provider module=%s provider=%s package_sha256=%s\n",
                    owner->module, owner->native ? "native" : "recipe", owner->package_sha256);
        }
    for (size_t i = 0; i < table->count; ++i)
        trace("publish-slot", i, table->slots[i].identity.module,
              table->slots[i].identity.export_name);
}

int on1x_kernel_import_init(const struct on1x_kernel_import *required, size_t count) {
    if (count && !required) return report_import(NULL, "missing required import table");
    if (count > ON1X_KERNEL_RESOLVE_MAX_IMPORTS ||
        count > (SIZE_MAX - sizeof(struct import_table)) / sizeof(struct import_slot))
        return report_import(required, "too many required exports");
    for (size_t i = 0; i < count; ++i)
        if (!on1x_kernel_import_valid(&required[i]))
            return report_import(&required[i], "invalid required import identity");

    struct import_table *existing = atomic_load_explicit(&published, memory_order_acquire);
    if (existing) return match_published(existing, required, count);
    if (!count) return 0;

    struct on1x_kernel_resolution *resolution = NULL;
    struct on1x_kernel_resolve_diag diag = {0};
    if (on1x_kernel_resolve(getenv("ON1X_KERNEL_PACKAGE_ROOTS"), required, count,
                           &resolution, &diag) != 0)
        return report_chain(*diag.module ? diag.module : required[0].module,
                            *diag.export_name ? diag.export_name : required[0].export_name,
                            *diag.reason ? diag.reason : "package resolution failed", diag.chain);
    struct import_table *stage = calloc(1, sizeof(*stage) + count * sizeof(stage->slots[0]));
    if (!stage) {
        on1x_kernel_resolution_free(resolution);
        return report_import(required, "out of memory staging kernel imports");
    }
    int status = stage_owners(stage, resolution, required, count);
    for (size_t i = 0; status == 0 && i < count; ++i)
        status = stage_slot(stage, i, resolution, &required[i]);
    if (status == 0) {
        /* An overlapping initializer may build a competing private table. Only
         * one can publish; the loser drops its owners and matches the winner. */
        if (atomic_compare_exchange_strong_explicit(&published, &existing, stage,
                                                     memory_order_acq_rel, memory_order_acquire)) {
            trace_publication(stage);
            /* Published slots own their strings and borrow only the checked
             * recipes and native mappings, not the original package bytes. */
            on1x_kernel_resolution_free(resolution);
            return 0;
        }
        status = match_published(existing, required, count);
    }
    rollback(stage);
    on1x_kernel_resolution_free(resolution);
    return status;
}

const struct on1x_kernel_import *on1x_kernel_import_identity(size_t slot) {
    const struct import_table *table = atomic_load_explicit(&published, memory_order_acquire);
    return table && slot < table->count ? &table->slots[slot].identity : NULL;
}

static const struct import_slot *dispatch_slot(size_t index, unsigned arity) {
    const struct import_table *table = atomic_load_explicit(&published, memory_order_acquire);
    if (!table || index >= table->count) {
        report_import(NULL, "invalid or unpublished dispatch slot");
        exit(70);
    }
    const struct import_slot *slot = &table->slots[index];
    if (slot->identity.arity != arity) {
        report_import(&slot->identity, "dispatch arity mismatch");
        exit(70);
    }
    return slot;
}

static void check_call(const struct import_slot *slot, int status) {
    if (status == ON1X_KERNEL_EVAL_OK) return;
    const char *reason = "checked kernel returned an invalid status";
    if (status == ON1X_KERNEL_EVAL_GUARD) reason = "checked kernel guard rejected arguments";
    if (status == ON1X_KERNEL_EVAL_OVERFLOW) reason = "checked kernel arithmetic overflow";
    if (status == ON1X_KERNEL_EVAL_INVALID) reason = "invalid checked kernel invocation";
    report_import(&slot->identity, reason);
    exit(70);
}

int64_t on1x_kernel_call2(size_t index, int64_t a, int64_t b) {
    const struct import_slot *slot = dispatch_slot(index, 2);
    int64_t out = 0;
    int status = slot->native.call2
        ? slot->native.call2(a, b, &out)
        : on1x_kernel_eval(slot->recipe, (const int64_t[]){a, b}, 2, &out);
    check_call(slot, status);
    return out;
}

int64_t on1x_kernel_call3(size_t index, int64_t a, int64_t b, int64_t c) {
    const struct import_slot *slot = dispatch_slot(index, 3);
    int64_t out = 0;
    int status = slot->native.call3
        ? slot->native.call3(a, b, c, &out)
        : on1x_kernel_eval(slot->recipe, (const int64_t[]){a, b, c}, 3, &out);
    check_call(slot, status);
    return out;
}
