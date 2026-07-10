#include "SwapchainUtils.h"
#include <cassert>
#include <limits>
#include <algorithm>
#include <cstdint>

// Vulkan
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

// Windowing Library
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace SwapchainUtils {
    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats) {
        assert(!availableFormats.empty());

        // Find format that supports 8-bit color & the SRGB color format
        vk::SurfaceFormatKHR chosenFormat{};
        for (auto format : availableFormats) {
            if (format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                chosenFormat = format;
                break;
            }
        }

        // just choose first if we can't find one
        if (chosenFormat.format == vk::Format::eUndefined) {
            chosenFormat = availableFormats[0];
        }

        return chosenFormat;
    }
    
    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes) {
        return vk::PresentModeKHR::eFifo;  // guaranteed to be available
    }

    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities, GLFWwindow *window) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        return {
            std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)
        };
    }

    uint32_t chooseSwapMinImageCount(const vk::SurfaceCapabilitiesKHR &capabilities) {
        // done because Khronos' official Vulkan tutorial uses this (go to swapchain section)
        uint32_t minImageCount{std::max(3u, capabilities.minImageCount)};

        // don't want to exceed maximum number of images
        const bool noMaximum{0 == capabilities.maxImageCount};
        if (!noMaximum && capabilities.maxImageCount < minImageCount) {
            minImageCount = capabilities.maxImageCount;
        }

        return minImageCount;
    }
}
