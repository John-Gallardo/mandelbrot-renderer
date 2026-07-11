#include "Config.h"
#include "App.h"
#include "SwapchainUtils.h"
#include <ranges>
#include <print>
#include <queue>
#include <utility>  // for std::pair
#include <stdexcept>
#include <vector>

// Vulkan
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

// Windowing Library
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

/********************
 * Public Functions *
 ********************/

void App::run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

/*********************
 * Private Functions *
 *********************/

/* Window */
void App::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // we don't want to create an OpenGL context
    m_window = glfwCreateWindow(Config::windowWidth, Config::windowHeight, Config::appTitle.data(), nullptr, nullptr);

    if (m_window == nullptr) {
        throw std::runtime_error("Failed to create GLFW window");
    }
}

/* Vulkan */
void App::initVulkan() {
    createInstance();
    createSurface();
    pickPhysicalDevice();
    createLogicalDeviceAndQueue();
    createSwapchain();
    createImageViews();
    createGraphicsPipeline();
}

void App::createInstance() {
    constexpr vk::ApplicationInfo appInfo{
        .pApplicationName  {Config::appTitle.data()},
        .applicationVersion{VK_MAKE_VERSION(1, 0, 0)},
        .engineVersion     {VK_MAKE_VERSION(1, 0, 0)},
        .apiVersion        {vk::ApiVersion14}
    };

    // Grab required instance extensions for GLFW
    uint32_t glfwExtensionCount{0};
    auto glfwExtensions        {glfwGetRequiredInstanceExtensions(&glfwExtensionCount)};
    auto extensionProperties   {m_context.enumerateInstanceExtensionProperties()};

    // Check if all extensions are supported by Vulkan implementation. Note: GLFW doesn't support range-based for loops
    for (uint32_t i : std::views::iota(0u, glfwExtensionCount)) {
        std::string currExtension {glfwExtensions[i]};
        bool containsCurrExtension{false};
        for (auto extension : extensionProperties) {
            if (currExtension == extension.extensionName) {
                containsCurrExtension = true;
                break;
            }
        }

        if (!containsCurrExtension) {
            throw std::runtime_error("Required GLFW extension not supported: " + std::string(currExtension));
        }
    }

    vk::InstanceCreateInfo createInfo{
        .pApplicationInfo       {&appInfo},
        .enabledExtensionCount  {glfwExtensionCount},
        .ppEnabledExtensionNames{glfwExtensions}
    };

    m_instance = vk::raii::Instance(m_context, createInfo);
}

void App::createSurface() {
    // Create C-style VkSurface & pass into Vulkan RAII wrapper
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(*m_instance, m_window, nullptr, &surface) != 0) {
        throw std::runtime_error("Failed to create a window surface!");
    }
    m_surface = vk::raii::SurfaceKHR(m_instance, surface);
}

void App::pickPhysicalDevice() {
    auto physicalDevices{m_instance.enumeratePhysicalDevices()};
    if (physicalDevices.empty()) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    std::priority_queue<std::pair<uint32_t, vk::raii::PhysicalDevice>> candidates;
    for (const auto &physicalDevice : physicalDevices) {
        auto deviceProperties{physicalDevice.getProperties()};
        uint32_t score       {0};

        /* Rank based on GPU type. dGPUs are/should be better than iGPUs, which is the reason for the scoring */
        switch (deviceProperties.deviceType) {
            case vk::PhysicalDeviceType::eDiscreteGpu:
                score += 1000;
                break;
            case vk::PhysicalDeviceType::eIntegratedGpu:
                score += 500;
                break;
            default:
                continue;
        }

        /* Check if Vulkan 1.4 is supported */
        bool supportsVulkan14{deviceProperties.apiVersion >= vk::ApiVersion14};
        if (!supportsVulkan14) {
            continue;
        }
        
        /* Check for graphics queue support */
        auto queueFamilies   {physicalDevice.getQueueFamilyProperties()};
        bool supportsGraphics{false};
        for (auto queueFamily : queueFamilies) {
            if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
                supportsGraphics = true;
                break;
            }
        }

        if (!supportsGraphics) {
            continue;
        }

        /* Check for extension support */
        std::vector<vk::ExtensionProperties> availableDeviceExtensions{physicalDevice.enumerateDeviceExtensionProperties()};
        bool supportsAllRequiredExtensions                            {true};
        for (const char *requiredExtension : m_requiredDeviceExtensions) {
            bool supportsRequiredExtension{false};
            for (auto deviceExtension : availableDeviceExtensions) {
                if (strcmp(deviceExtension.extensionName, requiredExtension) == 0) {
                    supportsRequiredExtension = true;
                    break;
                }
            }

            if (!supportsRequiredExtension) {
                supportsAllRequiredExtensions = false;
            }
        }

        if (!supportsAllRequiredExtensions) {
            continue;
        }

        /* Query for needed swapchain capabilities */
        std::vector<vk::SurfaceFormatKHR> availableFormats   {physicalDevice.getSurfaceFormatsKHR(*m_surface)};
        if (availableFormats.empty()) {
            continue;
        }

        /* Check for feature support */
        auto features{
            physicalDevice.template getFeatures2<
                vk::PhysicalDeviceFeatures2,
                vk::PhysicalDeviceVulkan11Features,
                vk::PhysicalDeviceVulkan13Features,
                vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
            >()
        };

        bool supportsRequiredFeatures{
            features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
            features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
            features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState
        };

        if (!supportsRequiredFeatures) {
            continue;
        }

        // If all requirements are supported, we finally add the device to the queue
        candidates.push({score, physicalDevice});
    }

    // Grab top candidate from our priority queue
    if (!candidates.empty()) {
        m_physicalDevice = candidates.top().second;
        std::println("Selected GPU: {}", m_physicalDevice.getProperties().deviceName.data());
    } else {
        throw std::runtime_error("Failed to find a dGPU or iGPU with support for features needed");
    }
}

void App::createLogicalDeviceAndQueue() {
    // Grab index of first queue family that supports graphics & presentation
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties{m_physicalDevice.getQueueFamilyProperties()};
    uint32_t queueFamilyIndex                                   {0xFFFFFFFF};
    for (auto [i, queueFamilyProperty] : std::views::enumerate(queueFamilyProperties)) {
        if ((queueFamilyProperty.queueFlags & vk::QueueFlagBits::eGraphics) &&
            m_physicalDevice.getSurfaceSupportKHR(i, *m_surface)) {
            queueFamilyIndex = i; 
            break;
        }
    }

    if (queueFamilyIndex == 0xFFFFFFFF) {
        throw std::runtime_error("Failed to find a queue that supports graphics");
    }

    // Specify features & extensions we want enabled
    vk::StructureChain<
        vk::PhysicalDeviceFeatures2,
        vk::PhysicalDeviceVulkan11Features,
        vk::PhysicalDeviceVulkan13Features,
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
    >
    featureChain{
        {},
        {.shaderDrawParameters{true}},
        {.dynamicRendering    {true}},
        {.extendedDynamicState{true}}
    };
    
    // Create logical device
    float queuePriority{1.0f};
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
        .queueFamilyIndex{queueFamilyIndex},
        .queueCount      {1},
        .pQueuePriorities{&queuePriority}
    };

    vk::DeviceCreateInfo deviceCreateInfo{
        .pNext                  {&featureChain.get<vk::PhysicalDeviceFeatures2>()},
        .queueCreateInfoCount   {1},
        .pQueueCreateInfos      {&deviceQueueCreateInfo},
        .enabledExtensionCount  {static_cast<uint32_t>(m_requiredDeviceExtensions.size())},
        .ppEnabledExtensionNames{m_requiredDeviceExtensions.data()}
    };
    
    m_device = vk::raii::Device(m_physicalDevice, deviceCreateInfo);

    // Retrieve queue handle
    m_graphicsQueue = vk::raii::Queue(m_device, queueFamilyIndex, 0);
}

void App::createSwapchain() {
    // Query for needed swapchain information
    vk::SurfaceCapabilitiesKHR surfaceCapabilities       {m_physicalDevice.getSurfaceCapabilitiesKHR(*m_surface)};
    std::vector<vk::SurfaceFormatKHR> availableFormats   {m_physicalDevice.getSurfaceFormatsKHR(*m_surface)};
    std::vector<vk::PresentModeKHR> availablePresentModes{m_physicalDevice.getSurfacePresentModesKHR(*m_surface)};

    // Grab information needed
    uint32_t minImageCount                 {SwapchainUtils::chooseSwapMinImageCount(surfaceCapabilities)};
    vk::PresentModeKHR swapChainPresentMode{SwapchainUtils::chooseSwapPresentMode(availablePresentModes)};
    m_swapChainExtent        = SwapchainUtils::chooseSwapExtent(surfaceCapabilities, m_window);
    m_swapChainSurfaceFormat = SwapchainUtils::chooseSwapSurfaceFormat(availableFormats);

    // Finally create swapchain
    vk::SwapchainCreateInfoKHR swapChainCreateInfo{
        .surface         {*m_surface},                                // the surface we use is the one we made
        .minImageCount   {minImageCount},                             // self-explanatory
        .imageFormat     {m_swapChainSurfaceFormat.format},           // 8-bit BGRA
        .imageColorSpace {m_swapChainSurfaceFormat.colorSpace},       // SRGB
        .imageExtent     {m_swapChainExtent},                         // window dimensions
        .imageArrayLayers{1},                                         // only 1 layer for a mandelbrot renderer
        .imageUsage      {vk::ImageUsageFlagBits::eColorAttachment},  // we render directly to the image
        .imageSharingMode{vk::SharingMode::eExclusive},               // image is owned exclusively by the graphics queue family. probably doesn't matter though since only 1 queue is used
        .preTransform    {surfaceCapabilities.currentTransform},      // we don't want any transformation applied
        .compositeAlpha  {vk::CompositeAlphaFlagBitsKHR::eOpaque},    // ignore alpha channel, we don't want to blend with other apps
        .presentMode     {swapChainPresentMode},                      // self-explanatory, I hard-coded it to be in FIFO (since it's guaranteed to be supported)
        .clipped         {true}                                       // don't care about pixels covered by another app
    };

    m_swapChain       = vk::raii::SwapchainKHR(m_device, swapChainCreateInfo);
    m_swapChainImages = m_swapChain.getImages();
}

void App::createImageViews() {
    // These 3 are identical for the images
    vk::ImageViewCreateInfo imageViewCreateInfo{
        .viewType{vk::ImageViewType::e2D},
        .format  {m_swapChainSurfaceFormat.format},
        .subresourceRange{
            .aspectMask    {vk::ImageAspectFlagBits::eColor},
            .baseMipLevel  {0},
            .levelCount    {1},
            .baseArrayLayer{0},
            .layerCount    {1}
        }
    };

    // Create image views
    for (const vk::Image &image : m_swapChainImages) {
        imageViewCreateInfo.image = image;
        swapChainImageViews.emplace_back(m_device, imageViewCreateInfo);
    }
}

void App::createGraphicsPipeline() {
    
}

/* Render Loop */
void App::mainLoop() {
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        processUserInput();
    }
}

void App::processUserInput() {
    if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(m_window, true);
    }
}

/* Cleanup */
void App::cleanup() {
    // for some reason I get a segfault unless I specifically set this to nullptr.
    // GDB's backtrace was showing my Nvidia driver's cleanup of this as the problem
    m_swapChain = nullptr;  
    glfwDestroyWindow(m_window);
    glfwTerminate();
}
