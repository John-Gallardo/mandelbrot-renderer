#pragma once

// Vulkan
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

// Windowing Library
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

class App {
    public:
        void run();

    private:
        GLFWwindow *m_window                     {nullptr};
        vk::raii::Context m_context;
        vk::raii::Instance m_instance            {nullptr};
        vk::raii::PhysicalDevice m_physicalDevice{nullptr};

        // Main functions
        void initWindow();
        void initVulkan();
        void mainLoop();
        void cleanup();

        // Helper functions
        void createInstance();

        /**
         * Picks a physical device.
         * Criteria:
         * 1. Must support Vulkan
         * 2. If Vulkan is supported, pick the first dGPU
         */
        void pickPhysicalDevice();

        void processUserInput();
};
