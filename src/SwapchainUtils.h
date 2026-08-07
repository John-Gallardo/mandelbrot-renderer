#pragma once
#include <cstdint>  // for uint32_t
#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>

namespace SwapchainUtils {
    // All are settings for the swapchain
    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats);
    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes);
    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities, GLFWwindow *window);
    uint32_t chooseSwapMinImageCount(const vk::SurfaceCapabilitiesKHR &capabilities);
}
