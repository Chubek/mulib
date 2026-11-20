#include "mulib-glx-vulkan.h"
#include <stdio.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

#define EXPECT(expr, expected) do { \
    VkResult actual = (expr); \
    if (actual != (expected)) { \
        fprintf(stderr, "%s:%d: %s returned %d, expected %d\n", \
                __FILE__, __LINE__, #expr, (int)actual, (int)(expected)); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void check_memory_type(void)
{
    const VkMemoryPropertyFlags visible = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    const VkMemoryPropertyFlags coherent = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    VkPhysicalDeviceMemoryProperties mem = {.memoryTypeCount = VK_MAX_MEMORY_TYPES};
    mem.memoryTypes[0].propertyFlags = visible;
    mem.memoryTypes[31].propertyFlags = visible | coherent;
    uint32_t index = 5;
    EXPECT(mulib_glx_vulkan_memory_type(&mem, 0, visible, coherent, &index),
           VK_ERROR_FEATURE_NOT_PRESENT);
    CHECK(index == 5);
    EXPECT(mulib_glx_vulkan_memory_type(&mem, UINT32_MAX, visible, coherent, &index),
           VK_SUCCESS);
    CHECK(index == 31);
    EXPECT(mulib_glx_vulkan_memory_type(&mem, 1, visible, coherent, &index), VK_SUCCESS);
    CHECK(index == 0);
    mem.memoryTypeCount++;
    EXPECT(mulib_glx_vulkan_memory_type(&mem, UINT32_MAX, visible, 0, &index),
           VK_ERROR_INITIALIZATION_FAILED);
}

static VKAPI_ATTR VkResult VKAPI_CALL wait_for_fault(
    VkDevice device, uint32_t count, const VkFence *fence, VkBool32 all, uint64_t timeout)
{
    (void)device;
    CHECK(count == 1 && fence && all == VK_TRUE);
    return timeout == 0 ? VK_TIMEOUT : VK_ERROR_DEVICE_LOST;
}

static VKAPI_ATTR VkResult VKAPI_CALL check_cache_range(
    VkDevice device, uint32_t count, const VkMappedMemoryRange *range)
{
    (void)device;
    CHECK(count == 1 && range && range->sType == VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE);
    CHECK(!range->pNext && range->offset == 0 && range->size == VK_WHOLE_SIZE);
    return VK_ERROR_OUT_OF_HOST_MEMORY;
}

/* Fake dispatch only for deterministic timeout/loss and noncoherent-range
 * behavior; never send these handles to a Vulkan loader or driver. */
static void check_dispatch_faults(void)
{
    mulib_glx_vulkan_device_t d = {.vk = {
        .WaitForFences = wait_for_fault,
        .FlushMappedMemoryRanges = check_cache_range,
        .InvalidateMappedMemoryRanges = check_cache_range
    }};
    mulib_glx_vulkan_commands_t c = {.device = &d, .state = MULIB_GLX_VK_COMMANDS_PENDING};
    EXPECT(mulib_glx_vulkan_commands_wait(&c, 0), VK_TIMEOUT);
    EXPECT(mulib_glx_vulkan_commands_close(&c), VK_NOT_READY);
    EXPECT(mulib_glx_vulkan_commands_wait(&c, 1), VK_ERROR_DEVICE_LOST);
    EXPECT(mulib_glx_vulkan_commands_begin(&c), VK_ERROR_DEVICE_LOST);
    EXPECT(mulib_glx_vulkan_commands_wait(&c, 0), VK_ERROR_INITIALIZATION_FAILED);

    int marker;
    mulib_glx_vulkan_buffer_t b = {
        .device = &d, .properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, .mapped = &marker
    };
    EXPECT(mulib_glx_vulkan_buffer_flush(&b), VK_ERROR_OUT_OF_HOST_MEMORY);
    EXPECT(mulib_glx_vulkan_buffer_invalidate(&b), VK_ERROR_OUT_OF_HOST_MEMORY);
}

static void check_transfer(mulib_glx_vulkan_device_t *d)
{
    enum { words = 513 };
    const VkDeviceSize size = words * sizeof(uint32_t);
    VkMemoryPropertyFlags visible = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    VkMemoryPropertyFlags coherent = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    mulib_glx_vulkan_buffer_t upload = {0}, scratch = {0}, download = {0};
    mulib_glx_vulkan_commands_t commands = {0};
    EXPECT(mulib_glx_vulkan_buffer_open(&upload, d, 0, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                      visible, coherent), VK_ERROR_INITIALIZATION_FAILED);
    EXPECT(mulib_glx_vulkan_buffer_open(&upload, d, size, 0, visible, coherent),
           VK_ERROR_INITIALIZATION_FAILED);
    EXPECT(mulib_glx_vulkan_buffer_open(&upload, d, size, UINT32_C(1) << 31, visible, coherent),
           VK_ERROR_FEATURE_NOT_PRESENT);
    /* Vulkan memory requirements never allow lazily allocated types for buffers.
     * Exercise rollback after vkCreateBuffer succeeds, before memory allocation. */
    EXPECT(mulib_glx_vulkan_buffer_open(&upload, d, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                      VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT, 0),
           VK_ERROR_FEATURE_NOT_PRESENT);
    CHECK(!upload.handle && !upload.device && d->children == 0);
    EXPECT(mulib_glx_vulkan_buffer_open(&upload, d, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                      visible, coherent), VK_SUCCESS);
    EXPECT(mulib_glx_vulkan_buffer_open(&upload, d, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                      visible, coherent), VK_ERROR_INITIALIZATION_FAILED);
    EXPECT(mulib_glx_vulkan_buffer_open(&download, d, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                      visible, VK_MEMORY_PROPERTY_HOST_CACHED_BIT), VK_SUCCESS);
    EXPECT(mulib_glx_vulkan_buffer_open(&scratch, d, size,
                                      VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0), VK_SUCCESS);
    CHECK(upload.size == size && scratch.size == size && download.size == size);
    CHECK(upload.allocation_size >= size && scratch.allocation_size >= size &&
          download.allocation_size >= size);
    CHECK(scratch.properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    EXPECT(mulib_glx_vulkan_buffer_flush(&upload), VK_ERROR_INITIALIZATION_FAILED);
    EXPECT(mulib_glx_vulkan_buffer_invalidate(&download), VK_ERROR_INITIALIZATION_FAILED);
    void *src = NULL, *dst = NULL;
    EXPECT(mulib_glx_vulkan_buffer_map(&upload, &src), VK_SUCCESS);
    EXPECT(mulib_glx_vulkan_buffer_map(&download, &dst), VK_SUCCESS);
    CHECK(src && dst);
    int sentinel;
    void *unchanged = &sentinel;
    EXPECT(mulib_glx_vulkan_buffer_map(&upload, &unchanged), VK_ERROR_INITIALIZATION_FAILED);
    CHECK(unchanged == &sentinel);
    if (!(scratch.properties & visible)) {
        EXPECT(mulib_glx_vulkan_buffer_map(&scratch, &unchanged), VK_ERROR_MEMORY_MAP_FAILED);
        CHECK(unchanged == &sentinel);
    }

    EXPECT(mulib_glx_vulkan_commands_open(&commands, d), VK_SUCCESS);
    EXPECT(mulib_glx_vulkan_commands_open(&commands, d), VK_ERROR_INITIALIZATION_FAILED);
    EXPECT(mulib_glx_vulkan_device_close(d), VK_NOT_READY);
    EXPECT(mulib_glx_vulkan_commands_submit(&commands), VK_ERROR_INITIALIZATION_FAILED);
    EXPECT(mulib_glx_vulkan_commands_wait(&commands, 0), VK_ERROR_INITIALIZATION_FAILED);
    for (uint32_t round = 1; round <= 2; ++round) {
        uint32_t expected[words];
        for (uint32_t i = 0; i < words; ++i)
            expected[i] = UINT32_C(0x9e3779b1) * (i + round);
        memcpy(src, expected, sizeof(expected));
        EXPECT(mulib_glx_vulkan_buffer_flush(&upload), VK_SUCCESS);
        EXPECT(mulib_glx_vulkan_commands_begin(&commands), VK_SUCCESS);
        EXPECT(mulib_glx_vulkan_commands_begin(&commands), VK_ERROR_INITIALIZATION_FAILED);
        const VkBufferCopy region = {.size = size};
        d->vk.CmdCopyBuffer(commands.handle, upload.handle, scratch.handle, 1, &region);
        const VkBufferMemoryBarrier device_readable = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = scratch.handle, .size = size
        };
        d->vk.CmdPipelineBarrier(commands.handle, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL,
                                 1, &device_readable, 0, NULL);
        d->vk.CmdCopyBuffer(commands.handle, scratch.handle, download.handle, 1, &region);
        const VkBufferMemoryBarrier host_readable = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT, .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = download.handle, .size = size
        };
        d->vk.CmdPipelineBarrier(commands.handle, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL,
                                 1, &host_readable, 0, NULL);
        EXPECT(mulib_glx_vulkan_commands_submit(&commands), VK_SUCCESS);
        EXPECT(mulib_glx_vulkan_commands_begin(&commands), VK_NOT_READY);
        EXPECT(mulib_glx_vulkan_commands_close(&commands), VK_NOT_READY);
        VkResult polled = mulib_glx_vulkan_commands_wait(&commands, 0);
        CHECK(polled == VK_SUCCESS || polled == VK_TIMEOUT);
        if (polled == VK_TIMEOUT)
            EXPECT(mulib_glx_vulkan_commands_wait(&commands, UINT64_C(10000000000)), VK_SUCCESS);
        EXPECT(mulib_glx_vulkan_buffer_invalidate(&download), VK_SUCCESS);
        CHECK(memcmp(expected, dst, sizeof(expected)) == 0);
    }
    EXPECT(mulib_glx_vulkan_commands_close(&commands), VK_SUCCESS);
    EXPECT(mulib_glx_vulkan_commands_close(&commands), VK_SUCCESS);
    mulib_glx_vulkan_buffer_unmap(&upload);
    CHECK(!upload.mapped);
    mulib_glx_vulkan_buffer_unmap(&upload);
    mulib_glx_vulkan_buffer_close(&download); /* Also unmaps. */
    mulib_glx_vulkan_buffer_close(&scratch);
    mulib_glx_vulkan_buffer_close(&upload);
    mulib_glx_vulkan_buffer_close(&upload);
    CHECK(!upload.handle && !scratch.handle && !download.handle && !commands.handle && d->children == 0);
}

int main(void)
{
    mulib_glx_vulkan_instance_t instance = {0};
    mulib_glx_vulkan_device_t device = {0};
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    uint32_t count = 0, family = UINT32_MAX;
    check_memory_type();
    check_dispatch_faults();
    EXPECT(mulib_glx_vulkan_instance_open(NULL, NULL), VK_ERROR_INITIALIZATION_FAILED);
    const VkInstanceCreateInfo wrong_type = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    EXPECT(mulib_glx_vulkan_instance_open(&instance, &wrong_type), VK_ERROR_INITIALIZATION_FAILED);
    EXPECT(mulib_glx_vulkan_physical_devices(&instance, &count, NULL),
           VK_ERROR_INITIALIZATION_FAILED);
    EXPECT(mulib_glx_vulkan_device_open(&device, &instance, physical, family, NULL),
           VK_ERROR_INITIALIZATION_FAILED);

    VkResult result = mulib_glx_vulkan_instance_open(&instance, NULL);
    if (result == VK_ERROR_INCOMPATIBLE_DRIVER) {
        puts("SKIP: Vulkan loader or compatible ICD unavailable");
        return 77;
    }
    EXPECT(result, VK_SUCCESS);
    EXPECT(mulib_glx_vulkan_instance_open(&instance, NULL), VK_ERROR_INITIALIZATION_FAILED);
    CHECK(mulib_glx_vulkan_instance_proc(&instance, "vkEnumeratePhysicalDevices"));
    CHECK(!mulib_glx_vulkan_instance_proc(&instance, "vkNoSuchMULIBCommand"));
    EXPECT(mulib_glx_vulkan_physical_devices(&instance, &count, NULL), VK_SUCCESS);
    if (!count) {
        EXPECT(mulib_glx_vulkan_instance_close(&instance), VK_SUCCESS);
        puts("SKIP: Vulkan loader found no physical devices");
        return 77;
    }
    EXPECT(mulib_glx_vulkan_physical_devices(&instance, NULL, NULL), VK_ERROR_INITIALIZATION_FAILED);
    VkPhysicalDevice single = VK_NULL_HANDLE;
    uint32_t n = 0;
    EXPECT(mulib_glx_vulkan_physical_devices(&instance, &n, &single), VK_INCOMPLETE);
    CHECK(n == 0 && !single);
    EXPECT(mulib_glx_vulkan_pick_queue(&instance, UINT32_C(1) << 31, &physical, &family),
           VK_ERROR_FEATURE_NOT_PRESENT);
    CHECK(!physical && family == UINT32_MAX);
    EXPECT(mulib_glx_vulkan_pick_queue(&instance, VK_QUEUE_TRANSFER_BIT, &physical, &family),
           VK_SUCCESS);
    EXPECT(mulib_glx_vulkan_queue_families(&instance, physical, &n, NULL), VK_SUCCESS);
    CHECK(family < n);
    VkQueueFamilyProperties *families = calloc(n, sizeof(*families));
    CHECK(families);
    EXPECT(mulib_glx_vulkan_queue_families(&instance, physical, &n, families), VK_SUCCESS);
    CHECK(family < n && families[family].queueCount > 0);
    CHECK(families[family].queueFlags &
          (VK_QUEUE_TRANSFER_BIT | VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT));
    free(families);

    EXPECT(mulib_glx_vulkan_device_open(&device, &instance, physical, n, NULL),
           VK_ERROR_INITIALIZATION_FAILED);
    const mulib_glx_vulkan_device_options_t bad_options = {.extension_count = 1};
    EXPECT(mulib_glx_vulkan_device_open(&device, &instance, physical, family, &bad_options),
           VK_ERROR_INITIALIZATION_FAILED);
    const char *bad_name = "VK_MULIB_intentional_missing_extension";
    const mulib_glx_vulkan_device_options_t missing_extension = {
        .extension_count = 1, .extensions = &bad_name
    };
    EXPECT(mulib_glx_vulkan_device_open(&device, &instance, physical, family, &missing_extension),
           VK_ERROR_EXTENSION_NOT_PRESENT);
    CHECK(!device.handle && instance.devices == 0);
    EXPECT(mulib_glx_vulkan_device_open(&device, &instance, physical, family, NULL), VK_SUCCESS);
    CHECK(device.queue && device.physical == physical && device.queue_family == family);
    CHECK(mulib_glx_vulkan_device_proc(&device, "vkQueueSubmit"));
    CHECK(!mulib_glx_vulkan_device_proc(&device, "vkNoSuchMULIBCommand"));
    EXPECT(mulib_glx_vulkan_instance_close(&instance), VK_NOT_READY);
    EXPECT(mulib_glx_vulkan_device_open(&device, &instance, physical, family, NULL),
           VK_ERROR_INITIALIZATION_FAILED);
    VkPhysicalDeviceProperties properties;
    instance.GetPhysicalDeviceProperties(physical, &properties);
    CHECK(properties.deviceName[0]);
    check_transfer(&device);
    EXPECT(mulib_glx_vulkan_device_close(&device), VK_SUCCESS);
    EXPECT(mulib_glx_vulkan_device_close(&device), VK_SUCCESS);
    CHECK(!device.handle && !device.queue && !instance.devices);
    EXPECT(mulib_glx_vulkan_instance_close(&instance), VK_SUCCESS);
    EXPECT(mulib_glx_vulkan_instance_close(&instance), VK_SUCCESS);
    CHECK(!instance.loader.lib && !instance.handle);
    const VkInstanceCreateInfo missing_instance_extension = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .enabledExtensionCount = 1, .ppEnabledExtensionNames = &bad_name
    };
    EXPECT(mulib_glx_vulkan_instance_open(&instance, &missing_instance_extension),
           VK_ERROR_EXTENSION_NOT_PRESENT);
    CHECK(!instance.loader.lib && !instance.handle);
    printf("PASS: Vulkan headless buffer round trips on %s (queue family %u)\n",
           properties.deviceName, (unsigned)family);
    return 0;
}
