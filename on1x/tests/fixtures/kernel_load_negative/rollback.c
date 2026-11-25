/* Link with --wrap=on1x_kernel_import_init to test the actual generated
 * requirements and observe publication without triggering the call trap. */
#define _POSIX_C_SOURCE 200809L
#include "on1x_kernel_import.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int __real_on1x_kernel_import_init(const struct on1x_kernel_import *, size_t);

static int probe_failure(const char *reason)
{
    fprintf(stderr, "kernel rollback probe: %s\n", reason);
    return 99;
}

int __wrap_on1x_kernel_import_init(const struct on1x_kernel_import *required,
                                   size_t count)
{
    if (count < 2)
        return probe_failure("fixture needs at least two import slots");
    if (setenv("ON1X_KERNEL_IMPORT_FAIL_SLOT", "1", 1) != 0)
        return probe_failure("cannot enable post-bind failure");
    if (__real_on1x_kernel_import_init(required, count) == 0)
        return probe_failure("injected initialization unexpectedly succeeded");
    for (size_t i = 0; i < count; ++i)
        if (on1x_kernel_import_identity(i) != NULL)
            return probe_failure("failed transaction published a slot");
    if (unsetenv("ON1X_KERNEL_IMPORT_FAIL_SLOT") != 0)
        return probe_failure("cannot clear post-bind failure");
    if (__real_on1x_kernel_import_init(required, count) != 0)
        return probe_failure("clean retry failed");
    for (size_t i = 0; i < count; ++i) {
        const struct on1x_kernel_import *id = on1x_kernel_import_identity(i);
        if (id == NULL || id->version != required[i].version
            || id->arity != required[i].arity
            || strcmp(id->module, required[i].module) != 0
            || strcmp(id->export_name, required[i].export_name) != 0
            || strcmp(id->interface_sha256, required[i].interface_sha256) != 0
            || strcmp(id->behavior_sha256, required[i].behavior_sha256) != 0)
            return probe_failure("retry did not publish the exact identity");
    }
    if (on1x_kernel_import_identity(count) != NULL)
        return probe_failure("out-of-range slot exists");
    return 0;
}
