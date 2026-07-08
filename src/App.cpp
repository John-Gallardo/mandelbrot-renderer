#include "Config.h"
#include "App.h"
#include <ranges>
#include <print>

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
    auto glfwExtensions        {glfwGetRequiredInstanceExtensions(&glfwExtensionCount)}; auto extensionProperties   {m_context.enumerateInstanceExtensionProperties()};

    // Check if all extensions are supported by Vulkan implementation
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

