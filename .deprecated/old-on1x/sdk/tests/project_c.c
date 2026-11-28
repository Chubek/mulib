#include "On1xSDK-Manifest.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static size_t live_allocations;

static void *allocate(void *context, size_t size, size_t alignment)
{
    (void)context;
    if (alignment > _Alignof(max_align_t))
        return NULL;
    void *result = malloc(size ? size : 1);
    if (result)
        ++live_allocations;
    return result;
}

static void *reallocate_unused(void *context, void *pointer, size_t old_size,
                               size_t new_size, size_t alignment)
{
    (void)context;
    (void)pointer;
    (void)old_size;
    (void)new_size;
    (void)alignment;
    abort();
}

static void release(void *context, void *pointer, size_t size, size_t alignment)
{
    (void)context;
    (void)size;
    (void)alignment;
    if (!pointer || live_allocations == 0)
        abort();
    --live_allocations;
    free(pointer);
}

int main(void)
{
    static const char source[] =
        "on1xfile 1\n"
        "kind executable\n"
        "name \"c_api\"\n"
        "target \"c_api\" \"main.on1x\"\n";
    On1xSDK_ProjectAllocator allocator = {
        ON1X_SDK_PROJECT_ABI_VERSION,
        sizeof(On1xSDK_ProjectAllocator),
        NULL,
        allocate,
        reallocate_unused,
        release
    };
    On1xSDK_ProjectManifest *manifest = NULL;
    On1xSDK_ProjectBytes input = {
        (const unsigned char *)source, sizeof(source) - 1
    };
    if (on1x_sdk_manifest_parse(input, NULL, &allocator, NULL, &manifest) !=
            ON1X_SDK_OK ||
        !manifest)
        return 1;
    const On1xSDK_ManifestInfo *info = on1x_sdk_manifest_info(manifest);
    if (!info || info->kind != ON1X_SDK_PROJECT_EXECUTABLE ||
        info->name.size != 5 || memcmp(info->name.data, "c_api", 5) != 0 ||
        on1x_sdk_manifest_target_count(manifest) != 1)
        return 2;
    const On1xSDK_ManifestTarget *target =
        on1x_sdk_manifest_target_at(manifest, 0);
    if (!target || target->source.size != 9 ||
        memcmp(target->source.data, "main.on1x", 9) != 0)
        return 3;
    on1x_sdk_manifest_destroy(manifest);
    return live_allocations == 0 ? 0 : 4;
}
