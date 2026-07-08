#include "Config.h"
#include "App.h"

// Vulkan
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

// Windowing Library
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>


/* Public Functions */
void App::run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

/* Private Functions */
void App::initWindow() {
    glfwInit();
    // we don't want to set up an OpenGL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(Config::windowWidth, Config::windowHeight, Config::appTitle.data(), nullptr, nullptr);
}

// TODO
void App::initVulkan() {

}

void App::mainLoop() {
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }
}

void App::cleanup() {

}
