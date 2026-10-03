// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <switch.h>

#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_VI_NN
#include <vulkan/vulkan.h>

u32 __nx_applet_type = AppletType_Application;
size_t __nx_heap_size = 0;

extern VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
vk_icdGetInstanceProcAddr(VkInstance instance, const char* name);

#define LOAD_INSTANCE(instance, name) \
    ((PFN_##name)vk_icdGetInstanceProcAddr((instance), #name))
#define LOAD_DEVICE(get_proc, device, name) \
    ((PFN_##name)(get_proc)((device), #name))

static FILE* log_file;

static void fail(const char* stage, VkResult result) {
    if (log_file) {
        fprintf(log_file, "%s failed: %d\n", stage, result);
        fflush(log_file);
    }
}

static void log_stage(const char* stage) {
    if (log_file) {
        fprintf(log_file, "OK: %s\n", stage);
        fflush(log_file);
    }
}

int main(void) {
    log_file = fopen("sdmc:/WindWakerRecomp-vulkan-smoke.log", "w");
    if (log_file) {
        fputs("Wind Waker Recomp - NVK clear/present gate\n", log_file);
        fflush(log_file);
    }

    setenv("NVK_I_WANT_A_BROKEN_VULKAN_DRIVER", "1", 1);
    setenv("MESA_SHADER_CACHE_DISABLE", "1", 1);
    setenv("MESA_LOG_FILE", "sdmc:/WindWakerRecomp-vulkan-smoke-mesa.log", 1);

    PFN_vkCreateInstance create_instance =
        (PFN_vkCreateInstance)vk_icdGetInstanceProcAddr(NULL, "vkCreateInstance");
    if (!create_instance) {
        fail("vk_icdGetInstanceProcAddr(vkCreateInstance)", VK_ERROR_INITIALIZATION_FAILED);
        goto exit_loop;
    }

    const char* instance_extensions[] = {"VK_KHR_surface", "VK_NN_vi_surface"};
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "WindWakerRecomp NVK gate",
        .apiVersion = VK_API_VERSION_1_1,
    };
    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = 2,
        .ppEnabledExtensionNames = instance_extensions,
    };
    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = create_instance(&instance_info, NULL, &instance);
    if (result != VK_SUCCESS) {
        fail("vkCreateInstance", result);
        goto exit_loop;
    }
    log_stage("vkCreateInstance");

    PFN_vkEnumeratePhysicalDevices enumerate_devices =
        LOAD_INSTANCE(instance, vkEnumeratePhysicalDevices);
    PFN_vkGetPhysicalDeviceQueueFamilyProperties get_queue_families =
        LOAD_INSTANCE(instance, vkGetPhysicalDeviceQueueFamilyProperties);
    PFN_vkCreateDevice create_device = LOAD_INSTANCE(instance, vkCreateDevice);
    PFN_vkDestroyInstance destroy_instance = LOAD_INSTANCE(instance, vkDestroyInstance);
    PFN_vkDestroySurfaceKHR destroy_surface = LOAD_INSTANCE(instance, vkDestroySurfaceKHR);
    PFN_vkGetDeviceProcAddr get_device_proc = LOAD_INSTANCE(instance, vkGetDeviceProcAddr);
    PFN_vkCreateViSurfaceNN create_vi_surface = LOAD_INSTANCE(instance, vkCreateViSurfaceNN);
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR get_surface_caps =
        LOAD_INSTANCE(instance, vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR get_surface_formats =
        LOAD_INSTANCE(instance, vkGetPhysicalDeviceSurfaceFormatsKHR);
    if (!enumerate_devices || !get_queue_families || !create_device ||
        !destroy_instance || !destroy_surface ||
        !get_device_proc || !create_vi_surface || !get_surface_caps ||
        !get_surface_formats) {
        fail("Vulkan instance dispatch", VK_ERROR_INITIALIZATION_FAILED);
        goto exit_loop;
    }

    uint32_t physical_count = 1;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    result = enumerate_devices(instance, &physical_count, &physical);
    if (result != VK_SUCCESS || physical_count == 0) {
        fail("vkEnumeratePhysicalDevices", result);
        goto exit_loop;
    }
    log_stage("vkEnumeratePhysicalDevices");

    uint32_t family_count = 0;
    get_queue_families(physical, &family_count, NULL);
    VkQueueFamilyProperties families[8];
    if (family_count > 8) family_count = 8;
    get_queue_families(physical, &family_count, families);
    uint32_t family = UINT32_MAX;
    for (uint32_t i = 0; i < family_count; ++i) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            family = i;
            break;
        }
    }
    if (family == UINT32_MAX) {
        fail("graphics queue selection", VK_ERROR_INITIALIZATION_FAILED);
        goto exit_loop;
    }

    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = family,
        .queueCount = 1,
        .pQueuePriorities = &priority,
    };
    const char* device_extensions[] = {"VK_KHR_swapchain"};
    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = device_extensions,
    };
    VkDevice device = VK_NULL_HANDLE;
    result = create_device(physical, &device_info, NULL, &device);
    if (result != VK_SUCCESS) {
        fail("vkCreateDevice", result);
        goto exit_loop;
    }
    log_stage("vkCreateDevice");

    PFN_vkGetDeviceQueue get_queue = LOAD_DEVICE(get_device_proc, device, vkGetDeviceQueue);
    PFN_vkCreateSwapchainKHR create_swapchain = LOAD_DEVICE(get_device_proc, device, vkCreateSwapchainKHR);
    PFN_vkGetSwapchainImagesKHR get_images = LOAD_DEVICE(get_device_proc, device, vkGetSwapchainImagesKHR);
    PFN_vkAcquireNextImageKHR acquire_image = LOAD_DEVICE(get_device_proc, device, vkAcquireNextImageKHR);
    PFN_vkQueuePresentKHR present = LOAD_DEVICE(get_device_proc, device, vkQueuePresentKHR);
    PFN_vkCreateCommandPool create_pool = LOAD_DEVICE(get_device_proc, device, vkCreateCommandPool);
    PFN_vkAllocateCommandBuffers allocate_commands = LOAD_DEVICE(get_device_proc, device, vkAllocateCommandBuffers);
    PFN_vkResetCommandBuffer reset_command = LOAD_DEVICE(get_device_proc, device, vkResetCommandBuffer);
    PFN_vkBeginCommandBuffer begin_command = LOAD_DEVICE(get_device_proc, device, vkBeginCommandBuffer);
    PFN_vkCmdPipelineBarrier barrier = LOAD_DEVICE(get_device_proc, device, vkCmdPipelineBarrier);
    PFN_vkCmdClearColorImage clear_image = LOAD_DEVICE(get_device_proc, device, vkCmdClearColorImage);
    PFN_vkEndCommandBuffer end_command = LOAD_DEVICE(get_device_proc, device, vkEndCommandBuffer);
    PFN_vkQueueSubmit submit = LOAD_DEVICE(get_device_proc, device, vkQueueSubmit);
    PFN_vkQueueWaitIdle wait_idle = LOAD_DEVICE(get_device_proc, device, vkQueueWaitIdle);
    PFN_vkCreateSemaphore create_semaphore = LOAD_DEVICE(get_device_proc, device, vkCreateSemaphore);
    PFN_vkDestroySemaphore destroy_semaphore = LOAD_DEVICE(get_device_proc, device, vkDestroySemaphore);
    PFN_vkDestroyCommandPool destroy_pool = LOAD_DEVICE(get_device_proc, device, vkDestroyCommandPool);
    PFN_vkDestroySwapchainKHR destroy_swapchain = LOAD_DEVICE(get_device_proc, device, vkDestroySwapchainKHR);
    PFN_vkDestroyDevice destroy_device = LOAD_DEVICE(get_device_proc, device, vkDestroyDevice);
    if (!get_queue || !create_swapchain || !get_images || !acquire_image ||
        !present || !create_pool || !allocate_commands || !reset_command ||
        !begin_command || !barrier || !clear_image || !end_command || !submit ||
        !wait_idle || !create_semaphore || !destroy_semaphore || !destroy_pool ||
        !destroy_swapchain || !destroy_device) {
        fail("Vulkan device dispatch", VK_ERROR_INITIALIZATION_FAILED);
        goto exit_loop;
    }

    VkQueue queue = VK_NULL_HANDLE;
    get_queue(device, family, 0, &queue);
    VkViSurfaceCreateInfoNN vi_info = {
        .sType = VK_STRUCTURE_TYPE_VI_SURFACE_CREATE_INFO_NN,
        .window = nwindowGetDefault(),
    };
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    result = create_vi_surface(instance, &vi_info, NULL, &surface);
    if (result != VK_SUCCESS) {
        fail("vkCreateViSurfaceNN", result);
        goto exit_loop;
    }
    log_stage("vkCreateViSurfaceNN");

    VkSurfaceCapabilitiesKHR caps;
    result = get_surface_caps(physical, surface, &caps);
    if (result != VK_SUCCESS) {
        fail("vkGetPhysicalDeviceSurfaceCapabilitiesKHR", result);
        goto exit_loop;
    }
    uint32_t format_count = 0;
    result = get_surface_formats(physical, surface, &format_count, NULL);
    if (result != VK_SUCCESS || format_count == 0) {
        fail("vkGetPhysicalDeviceSurfaceFormatsKHR", result);
        goto exit_loop;
    }
    VkSurfaceFormatKHR formats[8];
    if (format_count > 8) format_count = 8;
    result = get_surface_formats(physical, surface, &format_count, formats);
    if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
        fail("vkGetPhysicalDeviceSurfaceFormatsKHR", result);
        goto exit_loop;
    }
    VkSurfaceFormatKHR format = formats[0];
    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) extent = (VkExtent2D){1280, 720};
    uint32_t image_count = caps.minImageCount < 2 ? 2 : caps.minImageCount;
    VkSwapchainCreateInfoKHR swapchain_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = image_count,
        .imageFormat = format.format,
        .imageColorSpace = format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
    };
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    result = create_swapchain(device, &swapchain_info, NULL, &swapchain);
    if (result != VK_SUCCESS) {
        fail("vkCreateSwapchainKHR", result);
        goto exit_loop;
    }

    VkImage images[8];
    uint32_t actual_images = 8;
    result = get_images(device, swapchain, &actual_images, images);
    if (result != VK_SUCCESS || actual_images == 0) {
        fail("vkGetSwapchainImagesKHR", result);
        goto exit_loop;
    }
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = family,
    };
    VkCommandPool pool;
    result = create_pool(device, &pool_info, NULL, &pool);
    if (result != VK_SUCCESS) {
        fail("vkCreateCommandPool", result);
        goto exit_loop;
    }
    VkCommandBuffer commands[8];
    VkCommandBufferAllocateInfo command_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = actual_images,
    };
    result = allocate_commands(device, &command_info, commands);
    if (result != VK_SUCCESS) {
        fail("vkAllocateCommandBuffers", result);
        goto exit_loop;
    }
    VkSemaphoreCreateInfo semaphore_info = {.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkSemaphore acquired, rendered;
    if (create_semaphore(device, &semaphore_info, NULL, &acquired) != VK_SUCCESS ||
        create_semaphore(device, &semaphore_info, NULL, &rendered) != VK_SUCCESS) {
        fail("vkCreateSemaphore", VK_ERROR_INITIALIZATION_FAILED);
        goto exit_loop;
    }

    if (log_file) {
        fprintf(log_file, "swapchain ready: %ux%u, %u images; entering present loop\n",
                extent.width, extent.height, actual_images);
        fflush(log_file);
    }

    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    uint32_t frame = 0;
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;

        uint32_t index = 0;
        result = acquire_image(device, swapchain, UINT64_MAX, acquired, VK_NULL_HANDLE, &index);
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) break;

        reset_command(commands[index], 0);
        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };
        begin_command(commands[index], &begin_info);
        VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkImageMemoryBarrier to_clear = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = images[index],
            .subresourceRange = range,
        };
        barrier(commands[index], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &to_clear);
        float phase = (float)(frame & 255u) / 255.0f;
        VkClearColorValue color = {.float32 = {phase, 0.12f, 1.0f - phase, 1.0f}};
        clear_image(commands[index], images[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    &color, 1, &range);
        VkImageMemoryBarrier to_present = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = images[index],
            .subresourceRange = range,
        };
        barrier(commands[index], VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &to_present);
        end_command(commands[index]);

        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &acquired,
            .pWaitDstStageMask = &wait_stage,
            .commandBufferCount = 1,
            .pCommandBuffers = &commands[index],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &rendered,
        };
        if (submit(queue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS) break;
        wait_idle(queue);
        VkPresentInfoKHR present_info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &rendered,
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &index,
        };
        result = present(queue, &present_info);
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) break;
        wait_idle(queue);
        ++frame;
        if (frame == 1 && log_file) {
            fputs("first frame presented\n", log_file);
            fflush(log_file);
        }
    }
    if (log_file) {
        fprintf(log_file, "present loop ended after %u frames with result %d\n", frame, result);
        fflush(log_file);
    }
    wait_idle(queue);
    destroy_semaphore(device, rendered, NULL);
    destroy_semaphore(device, acquired, NULL);
    destroy_pool(device, pool, NULL);
    destroy_swapchain(device, swapchain, NULL);
    destroy_surface(instance, surface, NULL);
    destroy_device(device, NULL);
    destroy_instance(instance, NULL);
    log_stage("clean Vulkan shutdown");
    if (log_file) fclose(log_file);
    return 0;

exit_loop:
    ;
    PadState exit_pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&exit_pad);
    while (appletMainLoop()) {
        padUpdate(&exit_pad);
        if (padGetButtonsDown(&exit_pad) & HidNpadButton_Plus) break;
    }
    if (log_file) fclose(log_file);
    return 0;
}
