#include "mulib-glx-vulkan.h"
#include <stdio.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); \
        exit(EXIT_FAILURE); \
    } \
} while (0)
#define VK_OK(expr) do { \
    VkResult result_ = (expr); \
    if (result_ != VK_SUCCESS) { \
        fprintf(stderr, "%s:%d: %s returned %d\n", __FILE__, __LINE__, #expr, (int)result_); \
        exit(EXIT_FAILURE); \
    } \
} while (0)
#define LOAD(name) \
    PFN_vk##name name = (PFN_vk##name)mulib_glx_vulkan_device_proc(&device, "vk" #name); \
    CHECK(name)

int main(int argc, char **argv)
{
    enum { words = 64 };
    uint32_t code[4096];
    CHECK(argc == 2);
    FILE *shader_file = fopen(argv[1], "rb");
    CHECK(shader_file);
    size_t shader_size = fread(code, 1, sizeof(code), shader_file);
    CHECK(feof(shader_file) && !ferror(shader_file));
    CHECK(fclose(shader_file) == 0);
    CHECK(shader_size >= 5 * sizeof(uint32_t) && shader_size % sizeof(uint32_t) == 0);
    CHECK(code[0] == UINT32_C(0x07230203));

    mulib_glx_vulkan_instance_t instance = {0};
    mulib_glx_vulkan_device_t device = {0};
    mulib_glx_vulkan_commands_t commands = {0};
    mulib_glx_vulkan_buffer_t buffer = {0};
    VkPhysicalDevice physical;
    uint32_t family;
    VkResult result = mulib_glx_vulkan_instance_open(&instance, NULL);
    if (result == VK_ERROR_INCOMPATIBLE_DRIVER) {
        puts("SKIP: Vulkan loader or compatible ICD unavailable");
        return 77;
    }
    VK_OK(result);
    result = mulib_glx_vulkan_pick_queue(&instance, VK_QUEUE_COMPUTE_BIT, &physical, &family);
    if (result == VK_ERROR_FEATURE_NOT_PRESENT) {
        VK_OK(mulib_glx_vulkan_instance_close(&instance));
        puts("SKIP: Vulkan compute queue unavailable");
        return 77;
    }
    VK_OK(result);
    VK_OK(mulib_glx_vulkan_device_open(&device, &instance, physical, family, NULL));
    VK_OK(mulib_glx_vulkan_buffer_open(&buffer, &device, words * sizeof(uint32_t),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
    void *mapped = NULL;
    VK_OK(mulib_glx_vulkan_buffer_map(&buffer, &mapped));
    uint32_t *data = mapped;
    for (uint32_t i = 0; i < words; ++i)
        data[i] = i * 11 + 2;
    VK_OK(mulib_glx_vulkan_buffer_flush(&buffer));

    LOAD(CreateShaderModule); LOAD(DestroyShaderModule);
    LOAD(CreateDescriptorSetLayout); LOAD(DestroyDescriptorSetLayout);
    LOAD(CreatePipelineLayout); LOAD(DestroyPipelineLayout);
    LOAD(CreateDescriptorPool); LOAD(DestroyDescriptorPool);
    LOAD(AllocateDescriptorSets); LOAD(UpdateDescriptorSets);
    LOAD(CreateComputePipelines); LOAD(DestroyPipeline);
    LOAD(CmdBindPipeline); LOAD(CmdBindDescriptorSets); LOAD(CmdDispatch);

    VkShaderModule shader;
    const VkShaderModuleCreateInfo shader_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = shader_size, .pCode = code
    };
    VK_OK(CreateShaderModule(device.handle, &shader_info, NULL, &shader));

    VkDescriptorSetLayout set_layout;
    const VkDescriptorSetLayoutBinding binding = {
        .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
    };
    const VkDescriptorSetLayoutCreateInfo set_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1, .pBindings = &binding
    };
    VK_OK(CreateDescriptorSetLayout(device.handle, &set_layout_info, NULL, &set_layout));

    VkPipelineLayout layout;
    const VkPipelineLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1, .pSetLayouts = &set_layout
    };
    VK_OK(CreatePipelineLayout(device.handle, &layout_info, NULL, &layout));

    VkPipeline pipeline;
    const VkComputePipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT, .module = shader, .pName = "main"
        },
        .layout = layout, .basePipelineIndex = -1
    };
    VK_OK(CreateComputePipelines(device.handle, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline));
    DestroyShaderModule(device.handle, shader, NULL);

    VkDescriptorPool pool;
    const VkDescriptorPoolSize pool_size = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
    const VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .maxSets = 1,
        .poolSizeCount = 1, .pPoolSizes = &pool_size
    };
    VK_OK(CreateDescriptorPool(device.handle, &pool_info, NULL, &pool));

    VkDescriptorSet set;
    const VkDescriptorSetAllocateInfo set_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = pool, .descriptorSetCount = 1, .pSetLayouts = &set_layout
    };
    VK_OK(AllocateDescriptorSets(device.handle, &set_info, &set));
    const VkDescriptorBufferInfo descriptor = {.buffer = buffer.handle, .range = buffer.size};
    const VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = set, .dstBinding = 0,
        .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &descriptor
    };
    UpdateDescriptorSets(device.handle, 1, &write, 0, NULL);

    VK_OK(mulib_glx_vulkan_commands_open(&commands, &device));
    VK_OK(mulib_glx_vulkan_commands_begin(&commands));
    CmdBindPipeline(commands.handle, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    CmdBindDescriptorSets(commands.handle, VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, 1, &set, 0, NULL);
    CmdDispatch(commands.handle, words / 16, 1, 1);
    const VkBufferMemoryBarrier readable = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT, .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = buffer.handle, .size = buffer.size
    };
    device.vk.CmdPipelineBarrier(commands.handle, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 1, &readable, 0, NULL);
    VK_OK(mulib_glx_vulkan_commands_submit(&commands));
    VK_OK(mulib_glx_vulkan_commands_wait(&commands, UINT64_C(10000000000)));
    VK_OK(mulib_glx_vulkan_buffer_invalidate(&buffer));
    for (uint32_t i = 0; i < words; ++i)
        CHECK(data[i] == (i * 11 + 2) * 3 + 17);

    VK_OK(mulib_glx_vulkan_commands_close(&commands));
    DestroyPipeline(device.handle, pipeline, NULL);
    DestroyDescriptorPool(device.handle, pool, NULL);
    DestroyPipelineLayout(device.handle, layout, NULL);
    DestroyDescriptorSetLayout(device.handle, set_layout, NULL);
    mulib_glx_vulkan_buffer_close(&buffer);
    VK_OK(mulib_glx_vulkan_device_close(&device));
    VK_OK(mulib_glx_vulkan_instance_close(&instance));
    puts("PASS: Vulkan compute shader and storage-buffer readback");
    return 0;
}
