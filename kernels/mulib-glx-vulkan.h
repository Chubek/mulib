/* MuLIB graphical extension: glx-vulkan. C11, header-only. */
#ifndef MULIB_GLX_VULKAN_H
#define MULIB_GLX_VULKAN_H

#include "mulib-glx-core.h"
#include <dlfcn.h>

/* Legacy Linux loader API. SDK headers are not needed for these three calls. */
typedef struct {
    void *lib, *get_instance_proc_addr;
} mulib_glx_vulkan_t;

static inline int mulib_glx_vulkan_open(mulib_glx_vulkan_t *v)
{
    if (!v)
        return 0;
    void *lib = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!lib)
        return 0;
    void *p = dlsym(lib, "vkGetInstanceProcAddr");
    if (!p) {
        dlclose(lib);
        return 0;
    }
    *v = (mulib_glx_vulkan_t){lib, p};
    return 1;
}

static inline void *mulib_glx_vulkan_symbol(const mulib_glx_vulkan_t *v,
                                         const char *name)
{
    return v && v->lib && name ? dlsym(v->lib, name) : NULL;
}

static inline void mulib_glx_vulkan_close(mulib_glx_vulkan_t *v)
{
    if (!v)
        return;
    if (v->lib)
        dlclose(v->lib);
    *v = (mulib_glx_vulkan_t){0};
}

#ifdef MULIB_GLX_VULKAN_ENABLE
#include <vulkan/vulkan_core.h>

/* Owners start at {0}, are not copyable or movable, and are externally
 * synchronized. Public handles and dispatch entries are borrowed, read-only.
 * Wrapper children must be closed before their parent; callers also own the
 * lifetimes of any additional objects created using the raw dispatch. */
typedef struct {
    mulib_glx_vulkan_t loader;
    VkInstance handle;
    PFN_vkGetInstanceProcAddr GetInstanceProcAddr;
    PFN_vkDestroyInstance DestroyInstance;
    PFN_vkEnumeratePhysicalDevices EnumeratePhysicalDevices;
    PFN_vkGetPhysicalDeviceProperties GetPhysicalDeviceProperties;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties GetPhysicalDeviceQueueFamilyProperties;
    PFN_vkGetPhysicalDeviceMemoryProperties GetPhysicalDeviceMemoryProperties;
    PFN_vkGetDeviceProcAddr GetDeviceProcAddr;
    PFN_vkCreateDevice CreateDevice;
    size_t devices;
} mulib_glx_vulkan_instance_t;

typedef struct {
    PFN_vkDestroyDevice DestroyDevice;
    PFN_vkGetDeviceQueue GetDeviceQueue;
    PFN_vkDeviceWaitIdle DeviceWaitIdle;
    PFN_vkCreateCommandPool CreateCommandPool;
    PFN_vkDestroyCommandPool DestroyCommandPool;
    PFN_vkResetCommandPool ResetCommandPool;
    PFN_vkAllocateCommandBuffers AllocateCommandBuffers;
    PFN_vkBeginCommandBuffer BeginCommandBuffer;
    PFN_vkEndCommandBuffer EndCommandBuffer;
    PFN_vkQueueSubmit QueueSubmit;
    PFN_vkCreateFence CreateFence;
    PFN_vkDestroyFence DestroyFence;
    PFN_vkResetFences ResetFences;
    PFN_vkWaitForFences WaitForFences;
    PFN_vkCreateBuffer CreateBuffer;
    PFN_vkDestroyBuffer DestroyBuffer;
    PFN_vkGetBufferMemoryRequirements GetBufferMemoryRequirements;
    PFN_vkAllocateMemory AllocateMemory;
    PFN_vkFreeMemory FreeMemory;
    PFN_vkBindBufferMemory BindBufferMemory;
    PFN_vkMapMemory MapMemory;
    PFN_vkUnmapMemory UnmapMemory;
    PFN_vkFlushMappedMemoryRanges FlushMappedMemoryRanges;
    PFN_vkInvalidateMappedMemoryRanges InvalidateMappedMemoryRanges;
    PFN_vkCmdCopyBuffer CmdCopyBuffer;
    PFN_vkCmdFillBuffer CmdFillBuffer;
    PFN_vkCmdPipelineBarrier CmdPipelineBarrier;
} mulib_glx_vulkan_dispatch_t;

typedef struct {
    mulib_glx_vulkan_instance_t *instance;
    VkPhysicalDevice physical;
    VkDevice handle;
    VkQueue queue;
    uint32_t queue_family;
    VkPhysicalDeviceMemoryProperties memory;
    mulib_glx_vulkan_dispatch_t vk;
    size_t children;
} mulib_glx_vulkan_device_t;

typedef struct {
    const void *p_next;
    uint32_t extension_count;
    const char *const *extensions;
    const VkPhysicalDeviceFeatures *features;
} mulib_glx_vulkan_device_options_t;

/* POSIX dlsym supports conversion to a function pointer. Use memcpy for the
 * Linux ELF representation to avoid a non-ISO C object-to-function cast. */
#define MULIB_GLX_VK_DLSYM(target, symbol) do { \
    void *mulib_glx_vk_addr = (symbol); \
    _Static_assert(sizeof(target) == sizeof(mulib_glx_vk_addr), \
                   "incompatible POSIX loader pointer representation"); \
    memcpy(&(target), &mulib_glx_vk_addr, sizeof(target)); \
} while (0)

/* With NULL info, create a headless Vulkan 1.0 instance, no layers/extensions.
 * VK_ERROR_INCOMPATIBLE_DRIVER includes an unavailable runtime loader. */
static inline VkResult mulib_glx_vulkan_instance_open(
    mulib_glx_vulkan_instance_t *out, const VkInstanceCreateInfo *info)
{
    const VkInstanceCreateInfo defaults = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO
    };
    mulib_glx_vulkan_instance_t v = {0};
    if (!out || out->handle || out->loader.lib)
        return VK_ERROR_INITIALIZATION_FAILED;
    if (!info)
        info = &defaults;
    if (info->sType != VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO ||
        (info->pApplicationInfo &&
         info->pApplicationInfo->sType != VK_STRUCTURE_TYPE_APPLICATION_INFO) ||
        (info->enabledLayerCount && !info->ppEnabledLayerNames) ||
        (info->enabledExtensionCount && !info->ppEnabledExtensionNames))
        return VK_ERROR_INITIALIZATION_FAILED;

    v.loader.lib = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!v.loader.lib)
        return VK_ERROR_INCOMPATIBLE_DRIVER;
    v.loader.get_instance_proc_addr = dlsym(v.loader.lib, "vkGetInstanceProcAddr");
    MULIB_GLX_VK_DLSYM(v.GetInstanceProcAddr, v.loader.get_instance_proc_addr);
    /* Resolve the loader's core trampoline first for cleanup even if the
     * newly created instance has an incomplete dispatch table. */
    MULIB_GLX_VK_DLSYM(v.DestroyInstance, dlsym(v.loader.lib, "vkDestroyInstance"));
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    if (!v.GetInstanceProcAddr || !v.DestroyInstance)
        goto fail;
    PFN_vkCreateInstance create = (PFN_vkCreateInstance)
        v.GetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance");
    if (!create)
        goto fail;
    result = create(info, NULL, &v.handle);
    if (result != VK_SUCCESS)
        goto fail;
    if (!v.handle) {
        result = VK_ERROR_INITIALIZATION_FAILED;
        goto fail;
    }

    PFN_vkDestroyInstance destroy = (PFN_vkDestroyInstance)
        v.GetInstanceProcAddr(v.handle, "vkDestroyInstance");
    if (!destroy)
        goto missing;
    v.DestroyInstance = destroy;
#define MULIB_GLX_VK_LOAD_INSTANCE(name) do { \
    v.name = (PFN_vk##name)v.GetInstanceProcAddr(v.handle, "vk" #name); \
    if (!v.name) goto missing; \
} while (0)
    MULIB_GLX_VK_LOAD_INSTANCE(EnumeratePhysicalDevices);
    MULIB_GLX_VK_LOAD_INSTANCE(GetPhysicalDeviceProperties);
    MULIB_GLX_VK_LOAD_INSTANCE(GetPhysicalDeviceQueueFamilyProperties);
    MULIB_GLX_VK_LOAD_INSTANCE(GetPhysicalDeviceMemoryProperties);
    MULIB_GLX_VK_LOAD_INSTANCE(GetDeviceProcAddr);
    MULIB_GLX_VK_LOAD_INSTANCE(CreateDevice);
#undef MULIB_GLX_VK_LOAD_INSTANCE
    *out = v;
    return VK_SUCCESS;

missing:
    v.DestroyInstance(v.handle, NULL);
    result = VK_ERROR_INITIALIZATION_FAILED;
fail:
    mulib_glx_vulkan_close(&v.loader);
    return result;
}

/* VK_NOT_READY means the owner and its borrowed handles remain live. */
static inline VkResult mulib_glx_vulkan_instance_close(mulib_glx_vulkan_instance_t *v)
{
    if (!v)
        return VK_ERROR_INITIALIZATION_FAILED;
    if (v->devices)
        return VK_NOT_READY;
    if (v->handle)
        v->DestroyInstance(v->handle, NULL);
    mulib_glx_vulkan_close(&v->loader);
    *v = (mulib_glx_vulkan_instance_t){0};
    return VK_SUCCESS;
}

static inline PFN_vkVoidFunction mulib_glx_vulkan_instance_proc(
    const mulib_glx_vulkan_instance_t *v, const char *name)
{
    return v && v->handle && name ? v->GetInstanceProcAddr(v->handle, name) : NULL;
}

/* Native count/capacity semantics, including VK_INCOMPLETE. */
static inline VkResult mulib_glx_vulkan_physical_devices(
    const mulib_glx_vulkan_instance_t *v, uint32_t *count, VkPhysicalDevice *devices)
{
    if (!v || !v->handle || !count)
        return VK_ERROR_INITIALIZATION_FAILED;
    return v->EnumeratePhysicalDevices(v->handle, count, devices);
}

/* Native count/capacity semantics. A physical handle must come from v. */
static inline VkResult mulib_glx_vulkan_queue_families(
    const mulib_glx_vulkan_instance_t *v, VkPhysicalDevice physical,
    uint32_t *count, VkQueueFamilyProperties *families)
{
    if (!v || !v->handle || !physical || !count)
        return VK_ERROR_INITIALIZATION_FAILED;
    v->GetPhysicalDeviceQueueFamilyProperties(physical, count, families);
    return VK_SUCCESS;
}

/* First compatible device/family in enumeration order. Output remains
 * unchanged on failure. Graphics or compute queues implicitly support transfer. */
static inline VkResult mulib_glx_vulkan_pick_queue(
    const mulib_glx_vulkan_instance_t *v, VkQueueFlags required,
    VkPhysicalDevice *physical, uint32_t *family)
{
    if (!v || !v->handle || !physical || !family)
        return VK_ERROR_INITIALIZATION_FAILED;
    VkPhysicalDevice *devices = NULL;
    VkQueueFamilyProperties *families = NULL;
    size_t device_cap = 0, family_cap = 0;
    void *storage = NULL;
    uint32_t count = 0;
    VkResult result = VK_INCOMPLETE;
    for (unsigned attempt = 0; attempt < 4 && result == VK_INCOMPLETE; ++attempt) {
        result = mulib_glx_vulkan_physical_devices(v, &count, NULL);
        if (result != VK_SUCCESS)
            goto done;
        if (!count) {
            result = VK_ERROR_FEATURE_NOT_PRESENT;
            goto done;
        }
        storage = devices;
        if (!mgx_grow(&storage, &device_cap, count, sizeof(*devices))) {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
            goto done;
        }
        devices = storage;
        result = mulib_glx_vulkan_physical_devices(v, &count, devices);
    }
    if (result != VK_SUCCESS)
        goto done;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t n = 0;
        v->GetPhysicalDeviceQueueFamilyProperties(devices[i], &n, NULL);
        if (!n)
            continue;
        storage = families;
        if (!mgx_grow(&storage, &family_cap, n, sizeof(*families))) {
            result = VK_ERROR_OUT_OF_HOST_MEMORY;
            goto done;
        }
        families = storage;
        v->GetPhysicalDeviceQueueFamilyProperties(devices[i], &n, families);
        for (uint32_t j = 0; j < n; ++j) {
            VkQueueFlags flags = families[j].queueFlags;
            if (flags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))
                flags |= VK_QUEUE_TRANSFER_BIT;
            if (families[j].queueCount && (flags & required) == required) {
                *physical = devices[i];
                *family = j;
                goto done;
            }
        }
    }
    result = VK_ERROR_FEATURE_NOT_PRESENT;
done:
    free(families);
    free(devices);
    return result;
}

/* One unprotected queue, index zero. A physical handle must come from instance;
 * use the raw Vulkan create entry for other queue/device configurations. */
static inline VkResult mulib_glx_vulkan_device_open(
    mulib_glx_vulkan_device_t *out, mulib_glx_vulkan_instance_t *instance,
    VkPhysicalDevice physical, uint32_t family,
    const mulib_glx_vulkan_device_options_t *options)
{
    if (!out || out->handle || out->instance || !instance || !instance->handle ||
        !physical || (options && options->extension_count && !options->extensions))
        return VK_ERROR_INITIALIZATION_FAILED;
    if (instance->devices == SIZE_MAX)
        return VK_ERROR_TOO_MANY_OBJECTS;
    uint32_t count = 0;
    instance->GetPhysicalDeviceQueueFamilyProperties(physical, &count, NULL);
    if (family >= count)
        return VK_ERROR_INITIALIZATION_FAILED;
    const float priority = 1.0f;
    const VkDeviceQueueCreateInfo queue = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = family, .queueCount = 1, .pQueuePriorities = &priority
    };
    const VkDeviceCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = options ? options->p_next : NULL,
        .queueCreateInfoCount = 1, .pQueueCreateInfos = &queue,
        .enabledExtensionCount = options ? options->extension_count : 0,
        .ppEnabledExtensionNames = options ? options->extensions : NULL,
        .pEnabledFeatures = options ? options->features : NULL
    };
    mulib_glx_vulkan_device_t d = {
        .instance = instance, .physical = physical, .queue_family = family
    };
    /* Keep a loader trampoline available if scoped dispatch resolution fails. */
    PFN_vkDestroyDevice fallback = (PFN_vkDestroyDevice)
        mulib_glx_vulkan_instance_proc(instance, "vkDestroyDevice");
    if (!fallback)
        return VK_ERROR_INITIALIZATION_FAILED;
    VkResult result = instance->CreateDevice(physical, &info, NULL, &d.handle);
    if (result != VK_SUCCESS)
        return result;
    if (!d.handle)
        return VK_ERROR_INITIALIZATION_FAILED;
#define MULIB_GLX_VK_LOAD_DEVICE(name) do { \
    d.vk.name = (PFN_vk##name)instance->GetDeviceProcAddr(d.handle, "vk" #name); \
    if (!d.vk.name) goto missing; \
} while (0)
    MULIB_GLX_VK_LOAD_DEVICE(DestroyDevice);
    MULIB_GLX_VK_LOAD_DEVICE(GetDeviceQueue);
    MULIB_GLX_VK_LOAD_DEVICE(DeviceWaitIdle);
    MULIB_GLX_VK_LOAD_DEVICE(CreateCommandPool);
    MULIB_GLX_VK_LOAD_DEVICE(DestroyCommandPool);
    MULIB_GLX_VK_LOAD_DEVICE(ResetCommandPool);
    MULIB_GLX_VK_LOAD_DEVICE(AllocateCommandBuffers);
    MULIB_GLX_VK_LOAD_DEVICE(BeginCommandBuffer);
    MULIB_GLX_VK_LOAD_DEVICE(EndCommandBuffer);
    MULIB_GLX_VK_LOAD_DEVICE(QueueSubmit);
    MULIB_GLX_VK_LOAD_DEVICE(CreateFence);
    MULIB_GLX_VK_LOAD_DEVICE(DestroyFence);
    MULIB_GLX_VK_LOAD_DEVICE(ResetFences);
    MULIB_GLX_VK_LOAD_DEVICE(WaitForFences);
    MULIB_GLX_VK_LOAD_DEVICE(CreateBuffer);
    MULIB_GLX_VK_LOAD_DEVICE(DestroyBuffer);
    MULIB_GLX_VK_LOAD_DEVICE(GetBufferMemoryRequirements);
    MULIB_GLX_VK_LOAD_DEVICE(AllocateMemory);
    MULIB_GLX_VK_LOAD_DEVICE(FreeMemory);
    MULIB_GLX_VK_LOAD_DEVICE(BindBufferMemory);
    MULIB_GLX_VK_LOAD_DEVICE(MapMemory);
    MULIB_GLX_VK_LOAD_DEVICE(UnmapMemory);
    MULIB_GLX_VK_LOAD_DEVICE(FlushMappedMemoryRanges);
    MULIB_GLX_VK_LOAD_DEVICE(InvalidateMappedMemoryRanges);
    MULIB_GLX_VK_LOAD_DEVICE(CmdCopyBuffer);
    MULIB_GLX_VK_LOAD_DEVICE(CmdFillBuffer);
    MULIB_GLX_VK_LOAD_DEVICE(CmdPipelineBarrier);
#undef MULIB_GLX_VK_LOAD_DEVICE
    d.vk.GetDeviceQueue(d.handle, family, 0, &d.queue);
    if (!d.queue)
        goto missing;
    instance->GetPhysicalDeviceMemoryProperties(physical, &d.memory);
    *out = d;
    ++instance->devices;
    return VK_SUCCESS;
missing:
    (d.vk.DestroyDevice ? d.vk.DestroyDevice : fallback)(d.handle, NULL);
    return VK_ERROR_INITIALIZATION_FAILED;
}

#undef MULIB_GLX_VK_DLSYM

static inline PFN_vkVoidFunction mulib_glx_vulkan_device_proc(
    const mulib_glx_vulkan_device_t *d, const char *name)
{
    return d && d->handle && name ? d->instance->GetDeviceProcAddr(d->handle, name) : NULL;
}

/* Wait idle and destroy. Device loss is reported but still releases the owner. */
static inline VkResult mulib_glx_vulkan_device_close(mulib_glx_vulkan_device_t *d)
{
    if (!d)
        return VK_ERROR_INITIALIZATION_FAILED;
    if (!d->handle)
        return VK_SUCCESS;
    if (d->children)
        return VK_NOT_READY;
    VkResult result = d->vk.DeviceWaitIdle(d->handle);
    if (result != VK_SUCCESS && result != VK_ERROR_DEVICE_LOST)
        return result;
    d->vk.DestroyDevice(d->handle, NULL);
    --d->instance->devices;
    *d = (mulib_glx_vulkan_device_t){0};
    return result;
}

typedef enum {
    MULIB_GLX_VK_COMMANDS_READY,
    MULIB_GLX_VK_COMMANDS_RECORDING,
    MULIB_GLX_VK_COMMANDS_PENDING,
    MULIB_GLX_VK_COMMANDS_LOST
} mulib_glx_vulkan_commands_state_t;

typedef struct {
    mulib_glx_vulkan_device_t *device;
    VkCommandPool pool;
    VkCommandBuffer handle;
    VkFence fence;
    mulib_glx_vulkan_commands_state_t state;
} mulib_glx_vulkan_commands_t;

/* Reusable primary command buffer and its fence. Do not independently submit
 * this buffer or touch its fence via the raw dispatch. */
static inline VkResult mulib_glx_vulkan_commands_open(
    mulib_glx_vulkan_commands_t *out, mulib_glx_vulkan_device_t *d)
{
    if (!out || out->device || !d || !d->handle)
        return VK_ERROR_INITIALIZATION_FAILED;
    if (d->children == SIZE_MAX)
        return VK_ERROR_TOO_MANY_OBJECTS;
    mulib_glx_vulkan_commands_t c = {.device = d};
    const VkCommandPoolCreateInfo pool = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = d->queue_family
    };
    VkResult result = d->vk.CreateCommandPool(d->handle, &pool, NULL, &c.pool);
    if (result != VK_SUCCESS)
        return result;
    const VkCommandBufferAllocateInfo buffer = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = c.pool, .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    result = d->vk.AllocateCommandBuffers(d->handle, &buffer, &c.handle);
    if (result == VK_SUCCESS) {
        const VkFenceCreateInfo fence = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        result = d->vk.CreateFence(d->handle, &fence, NULL, &c.fence);
    }
    if (result != VK_SUCCESS) {
        d->vk.DestroyCommandPool(d->handle, c.pool, NULL);
        return result;
    }
    *out = c;
    ++d->children;
    return VK_SUCCESS;
}

static inline VkResult mulib_glx_vulkan_commands_begin(mulib_glx_vulkan_commands_t *c)
{
    if (!c || !c->device || c->state == MULIB_GLX_VK_COMMANDS_RECORDING)
        return VK_ERROR_INITIALIZATION_FAILED;
    if (c->state == MULIB_GLX_VK_COMMANDS_PENDING)
        return VK_NOT_READY;
    if (c->state == MULIB_GLX_VK_COMMANDS_LOST)
        return VK_ERROR_DEVICE_LOST;
    const mulib_glx_vulkan_device_t *d = c->device;
    VkResult result = d->vk.ResetCommandPool(d->handle, c->pool, 0);
    if (result != VK_SUCCESS) {
        if (result == VK_ERROR_DEVICE_LOST)
            c->state = MULIB_GLX_VK_COMMANDS_LOST;
        return result;
    }
    const VkCommandBufferBeginInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    result = d->vk.BeginCommandBuffer(c->handle, &info);
    if (result == VK_SUCCESS)
        c->state = MULIB_GLX_VK_COMMANDS_RECORDING;
    else if (result == VK_ERROR_DEVICE_LOST)
        c->state = MULIB_GLX_VK_COMMANDS_LOST;
    return result;
}

static inline VkResult mulib_glx_vulkan_commands_submit(mulib_glx_vulkan_commands_t *c)
{
    if (!c || !c->device || c->state != MULIB_GLX_VK_COMMANDS_RECORDING)
        return VK_ERROR_INITIALIZATION_FAILED;
    const mulib_glx_vulkan_device_t *d = c->device;
    c->state = MULIB_GLX_VK_COMMANDS_READY;
    VkResult result = d->vk.EndCommandBuffer(c->handle);
    if (result == VK_SUCCESS)
        result = d->vk.ResetFences(d->handle, 1, &c->fence);
    if (result != VK_SUCCESS) {
        if (result == VK_ERROR_DEVICE_LOST)
            c->state = MULIB_GLX_VK_COMMANDS_LOST;
        return result;
    }
    const VkSubmitInfo info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1, .pCommandBuffers = &c->handle
    };
    result = d->vk.QueueSubmit(d->queue, 1, &info, c->fence);
    /* For determining in-use objects, a device-lost submit counts as success. */
    if (result == VK_SUCCESS || result == VK_ERROR_DEVICE_LOST)
        c->state = MULIB_GLX_VK_COMMANDS_PENDING;
    return result;
}

/* Nanoseconds, including zero for a poll. A timeout leaves the work pending. */
static inline VkResult mulib_glx_vulkan_commands_wait(
    mulib_glx_vulkan_commands_t *c, uint64_t timeout)
{
    if (!c || !c->device || c->state != MULIB_GLX_VK_COMMANDS_PENDING)
        return VK_ERROR_INITIALIZATION_FAILED;
    const mulib_glx_vulkan_device_t *d = c->device;
    VkResult result = d->vk.WaitForFences(d->handle, 1, &c->fence, VK_TRUE, timeout);
    if (result == VK_SUCCESS)
        c->state = MULIB_GLX_VK_COMMANDS_READY;
    else if (result == VK_ERROR_DEVICE_LOST)
        c->state = MULIB_GLX_VK_COMMANDS_LOST;
    return result;
}

static inline VkResult mulib_glx_vulkan_commands_close(mulib_glx_vulkan_commands_t *c)
{
    if (!c)
        return VK_ERROR_INITIALIZATION_FAILED;
    if (!c->device)
        return VK_SUCCESS;
    if (c->state == MULIB_GLX_VK_COMMANDS_PENDING)
        return VK_NOT_READY;
    mulib_glx_vulkan_device_t *d = c->device;
    d->vk.DestroyFence(d->handle, c->fence, NULL);
    d->vk.DestroyCommandPool(d->handle, c->pool, NULL);
    --d->children;
    *c = (mulib_glx_vulkan_commands_t){0};
    return VK_SUCCESS;
}

typedef struct {
    mulib_glx_vulkan_device_t *device;
    VkBuffer handle;
    VkDeviceMemory memory;
    VkDeviceSize size;
    VkDeviceSize allocation_size;
    VkMemoryPropertyFlags properties;
    void *mapped;
} mulib_glx_vulkan_buffer_t;

/* Prefer a type satisfying all preferred properties, then any required match. */
static inline VkResult mulib_glx_vulkan_memory_type(
    const VkPhysicalDeviceMemoryProperties *memory, uint32_t allowed,
    VkMemoryPropertyFlags required, VkMemoryPropertyFlags preferred, uint32_t *index)
{
    if (!memory || !index || memory->memoryTypeCount > VK_MAX_MEMORY_TYPES)
        return VK_ERROR_INITIALIZATION_FAILED;
    uint32_t fallback = UINT32_MAX;
    for (uint32_t i = 0; i < memory->memoryTypeCount; ++i) {
        VkMemoryPropertyFlags flags = memory->memoryTypes[i].propertyFlags;
        if (!(allowed & (UINT32_C(1) << i)) || (flags & required) != required)
            continue;
        if ((flags & preferred) == preferred) {
            *index = i;
            return VK_SUCCESS;
        }
        if (fallback == UINT32_MAX)
            fallback = i;
    }
    if (fallback == UINT32_MAX)
        return VK_ERROR_FEATURE_NOT_PRESENT;
    *index = fallback;
    return VK_SUCCESS;
}

/* A single allocation bound at offset zero; Vulkan 1.0 usage bits only. */
static inline VkResult mulib_glx_vulkan_buffer_open(
    mulib_glx_vulkan_buffer_t *out, mulib_glx_vulkan_device_t *d,
    VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags required, VkMemoryPropertyFlags preferred)
{
    if (!out || out->device || !d || !d->handle || !size || !usage)
        return VK_ERROR_INITIALIZATION_FAILED;
    const VkBufferUsageFlags supported_usage =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
        VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT |
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
        VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    if (usage & ~supported_usage)
        return VK_ERROR_FEATURE_NOT_PRESENT;
    if (d->children == SIZE_MAX)
        return VK_ERROR_TOO_MANY_OBJECTS;
    mulib_glx_vulkan_buffer_t b = {.device = d, .size = size};
    const VkBufferCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size, .usage = usage, .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VkResult result = d->vk.CreateBuffer(d->handle, &info, NULL, &b.handle);
    if (result != VK_SUCCESS)
        return result;
    VkMemoryRequirements req;
    uint32_t type;
    d->vk.GetBufferMemoryRequirements(d->handle, b.handle, &req);
    uint32_t allowed = req.memoryTypeBits;
#ifdef VK_VERSION_1_1
    /* Binding protected memory to an unprotected buffer is invalid. */
    for (uint32_t i = 0; i < d->memory.memoryTypeCount && i < VK_MAX_MEMORY_TYPES; ++i)
        if (d->memory.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT)
            allowed &= ~(UINT32_C(1) << i);
#endif
    result = mulib_glx_vulkan_memory_type(&d->memory, allowed, required, preferred, &type);
    if (result != VK_SUCCESS)
        goto fail_buffer;
    const VkMemoryAllocateInfo alloc = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size, .memoryTypeIndex = type
    };
    b.allocation_size = req.size;
    b.properties = d->memory.memoryTypes[type].propertyFlags;
    result = d->vk.AllocateMemory(d->handle, &alloc, NULL, &b.memory);
    if (result != VK_SUCCESS)
        goto fail_buffer;
    result = d->vk.BindBufferMemory(d->handle, b.handle, b.memory, 0);
    if (result != VK_SUCCESS) {
        d->vk.DestroyBuffer(d->handle, b.handle, NULL);
        d->vk.FreeMemory(d->handle, b.memory, NULL);
        return result;
    }
    *out = b;
    ++d->children;
    return VK_SUCCESS;
fail_buffer:
    d->vk.DestroyBuffer(d->handle, b.handle, NULL);
    return result;
}

/* Map the entire allocation; use at most size bytes of the returned mapping. */
static inline VkResult mulib_glx_vulkan_buffer_map(
    mulib_glx_vulkan_buffer_t *b, void **out)
{
    if (!b || !b->device || !out || b->mapped)
        return VK_ERROR_INITIALIZATION_FAILED;
    if (!(b->properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) ||
        b->allocation_size > SIZE_MAX)
        return VK_ERROR_MEMORY_MAP_FAILED;
    mulib_glx_vulkan_device_t *d = b->device;
    void *mapped;
    VkResult result = d->vk.MapMemory(d->handle, b->memory, 0, VK_WHOLE_SIZE, 0, &mapped);
    if (result == VK_SUCCESS) {
        b->mapped = mapped;
        *out = mapped;
    }
    return result;
}

/* Cache operations cover the entire mapping and obey noncoherent atom
 * alignment. They do not synchronize access with device commands. */
static inline VkResult mulib_glx_vulkan_buffer_flush(const mulib_glx_vulkan_buffer_t *b)
{
    if (!b || !b->mapped)
        return VK_ERROR_INITIALIZATION_FAILED;
    if (b->properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        return VK_SUCCESS;
    const VkMappedMemoryRange range = {
        .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .memory = b->memory, .offset = 0, .size = VK_WHOLE_SIZE
    };
    return b->device->vk.FlushMappedMemoryRanges(b->device->handle, 1, &range);
}

static inline VkResult mulib_glx_vulkan_buffer_invalidate(
    const mulib_glx_vulkan_buffer_t *b)
{
    if (!b || !b->mapped)
        return VK_ERROR_INITIALIZATION_FAILED;
    if (b->properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
        return VK_SUCCESS;
    const VkMappedMemoryRange range = {
        .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        .memory = b->memory, .offset = 0, .size = VK_WHOLE_SIZE
    };
    return b->device->vk.InvalidateMappedMemoryRanges(b->device->handle, 1, &range);
}

static inline void mulib_glx_vulkan_buffer_unmap(mulib_glx_vulkan_buffer_t *b)
{
    if (b && b->mapped) {
        b->device->vk.UnmapMemory(b->device->handle, b->memory);
        b->mapped = NULL;
    }
}

/* Caller ensures the buffer is not in flight, even if it is mapped. */
static inline void mulib_glx_vulkan_buffer_close(mulib_glx_vulkan_buffer_t *b)
{
    if (!b || !b->device)
        return;
    mulib_glx_vulkan_device_t *d = b->device;
    mulib_glx_vulkan_buffer_unmap(b);
    d->vk.DestroyBuffer(d->handle, b->handle, NULL);
    d->vk.FreeMemory(d->handle, b->memory, NULL);
    --d->children;
    *b = (mulib_glx_vulkan_buffer_t){0};
}

#endif /* MULIB_GLX_VULKAN_ENABLE */
#endif /* MULIB_GLX_VULKAN_H */
