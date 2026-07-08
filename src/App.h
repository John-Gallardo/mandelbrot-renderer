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
        GLFWwindow *m_window         {nullptr};
        vk::raii::Context  m_context;
        vk::raii::Instance m_instance{nullptr};

        // Main functions
        void initWindow();
        void initVulkan();
        void mainLoop();
        void cleanup();

        // Helper functions
        void createInstance();
        void processUserInput();
};
