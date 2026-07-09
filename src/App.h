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
        vk::raii::Context m_context              {};
        vk::raii::Instance m_instance            {nullptr};
        vk::raii::SurfaceKHR m_surface           {nullptr};
        vk::raii::PhysicalDevice m_physicalDevice{nullptr};
        vk::raii::Device m_device                {nullptr};
        vk::raii::Queue m_graphicsQueue          {nullptr};
        std::vector<const char*> m_requiredDeviceExtensions{
            vk::KHRSwapchainExtensionName
        };

        // Main functions
        void initWindow();
        void initVulkan();
        void mainLoop();
        void cleanup();

        // Helper functions
        void createInstance();
        void createSurface();

        /**
         * Picks a physical device, using a priority queue
         * Criteria to be included:
         * 1. Must be a dGPU/iGPU
         * 2. Must support Vulkan 1.4
         * 3. Must support all required extensions
         * 4. Must support all required features
         */
        void pickPhysicalDevice();
        void processUserInput();
        void createLogicalDeviceAndQueue();
};
