#include "Config.h"
#include "App.h"
#include <ranges>
#include <print>
#include <queue>
#include <utility>  // for std::pair
#include <stdexcept>

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
    pickPhysicalDevice();
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

void App::pickPhysicalDevice() {
    auto physicalDevices{m_instance.enumeratePhysicalDevices()};
    if (physicalDevices.empty()) {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }

    std::priority_queue<std::pair<uint32_t, vk::raii::PhysicalDevice>> candidates;
    for (const auto &physicalDevice : physicalDevices) {
        auto deviceProperties{physicalDevice.getProperties()};
        uint32_t score       {0};

        // Rank based on GPU type. dGPUs are/should be better than iGPUs, which is the reason for the scoring
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

        // Check if Vulkan 1.4 is supported
        bool supportsVulkan14{deviceProperties.apiVersion >= vk::ApiVersion14};
        if (!supportsVulkan14) {
            continue;
        }
        
        // Check for graphics queue support
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

        // Check for extension support
        std::vector<const char*> requiredDeviceExtensions{
            vk::KHRSwapchainExtensionName
        };
        auto availableDeviceExtensions    {physicalDevice.enumerateDeviceExtensionProperties()};
        bool supportsAllRequiredExtensions{true};
        for (const char *requiredExtension : requiredDeviceExtensions) {
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

        // Check for feature support
        auto features{physicalDevice.template
            getFeatures2<vk::PhysicalDeviceFeatures2,
                        vk::PhysicalDeviceVulkan11Features,
                        vk::PhysicalDeviceVulkan13Features,
                        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>()
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

}
