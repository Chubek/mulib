/* Intentionally no Vulkan SDK, typed opt-in macro or link to libvulkan. */
#include "mulib-glx-vulkan.h"
#include <stdio.h>

int main(void)
{
    mulib_glx_vulkan_t loader = {0};
    if (mulib_glx_vulkan_symbol(&loader, "vkGetInstanceProcAddr") ||
        mulib_glx_vulkan_open(NULL))
        return 1;
    if (mulib_glx_vulkan_open(&loader)) {
        if (!loader.get_instance_proc_addr ||
            mulib_glx_vulkan_symbol(&loader, "vkGetInstanceProcAddr") != loader.get_instance_proc_addr ||
            mulib_glx_vulkan_symbol(&loader, NULL) ||
            mulib_glx_vulkan_symbol(&loader, "vkNoSuchMULIBCommand"))
            return 1;
        mulib_glx_vulkan_close(&loader);
    }
    mulib_glx_vulkan_close(&loader);
    if (loader.lib || loader.get_instance_proc_addr)
        return 1;
    puts("PASS: Vulkan loader-only API (no SDK headers or Vulkan linkage)");
    return 0;
}
