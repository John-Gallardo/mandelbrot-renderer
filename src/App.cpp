#include "Config.h"
#include "App.h"
#include "SwapchainUtils.h"
#include "PushConstants.h"
#include <ranges>
#include <print>
#include <queue>
#include <utility>  // for std::pair
#include <stdexcept>
#include <vector>
#include <fstream>
#include <vulkan/vulkan_raii.hpp>
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
    // NOTE: my version of renderdoc fails to create a GLFW surface with wayland. switching to this fixed it
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);  
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // we don't want to create an OpenGL context
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_window = glfwCreateWindow(Config::windowWidth, Config::windowHeight, Config::appTitle.data(), nullptr, nullptr);
    if (m_window == nullptr) {
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwSetWindowUserPointer(m_window, this);  // want to pass app to GLFW's callback function
    glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
}

void App::framebufferResizeCallback(GLFWwindow *window, int width, int height) {
    auto app{reinterpret_cast<App*>(glfwGetWindowUserPointer(window))};
    app->m_framebufferResized = true;
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
    createCommandPool();
    createCommandBuffers();
    createSyncObjects();
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
            features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
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
    for (auto [i, queueFamilyProperty] : std::views::enumerate(queueFamilyProperties)) {
        if ((queueFamilyProperty.queueFlags & vk::QueueFlagBits::eGraphics) &&
            m_physicalDevice.getSurfaceSupportKHR(i, *m_surface)) {
            m_graphicsQueueFamilyIndex = i; 
            break;
        }
    }

    if (m_graphicsQueueFamilyIndex == 0xFFFFFFFF) {
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

        {.synchronization2{true},
        .dynamicRendering {true}},

        {.extendedDynamicState{true}}
    };
    
    // Create logical device
    float queuePriority{1.0f};
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
        .queueFamilyIndex{m_graphicsQueueFamilyIndex},
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
    m_graphicsQueue = vk::raii::Queue(m_device, m_graphicsQueueFamilyIndex, 0);
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
        m_swapChainImageViews.emplace_back(m_device, imageViewCreateInfo);
    }
}

void App::createGraphicsPipeline() {
    // Create shader module
    std::vector<char> shaderCode       {readFile("src/shaders/slang.spv")};
    vk::raii::ShaderModule shaderModule{createShaderModule(shaderCode)};

    // Create shader stages for vertex & fragment shader
    vk::PipelineShaderStageCreateInfo vertShaderStageInfo{
        .stage {vk::ShaderStageFlagBits::eVertex},
        .module{shaderModule},
        .pName {"vertMain"}
    };

    vk::PipelineShaderStageCreateInfo fragShaderStageInfo{
        .stage {vk::ShaderStageFlagBits::eFragment},
        .module{shaderModule},
        .pName {"fragMain"}
    };

    constexpr int numShaderStages{2};
    std::array<vk::PipelineShaderStageCreateInfo, numShaderStages> shaderStages{vertShaderStageInfo, fragShaderStageInfo};

    // Specify how vertex input is passed in (nothing since vertex shader is hardcoded for now)
    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};

    // Specify how we are drawing our vertices
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
        .topology{vk::PrimitiveTopology::eTriangleList}
    };

    // Dynamic State
    std::vector<vk::DynamicState> dynamicStates{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount{static_cast<uint32_t>(dynamicStates.size())},
        .pDynamicStates   {dynamicStates.data()}
    };

    // Viewport & scissor test
    vk::PipelineViewportStateCreateInfo viewportState{
        .viewportCount{1},
        .scissorCount {1},
    };

    // Rasterizer
    vk::PipelineRasterizationStateCreateInfo rasterizer{
        .depthClampEnable       {vk::False},
        .rasterizerDiscardEnable{vk::False},
        .polygonMode            {vk::PolygonMode::eFill},
        .cullMode               {vk::CullModeFlagBits::eBack},
        .frontFace              {vk::FrontFace::eClockwise},
        .depthBiasEnable        {vk::False},
        .lineWidth              {1.0f}
    };

    // Multisampling
    vk::PipelineMultisampleStateCreateInfo multisampling{
        .rasterizationSamples{vk::SampleCountFlagBits::e1},
        .sampleShadingEnable {vk::False}
    };

    // NOTE: don't have anything for depth & stencil testing right now

    // Color blending
    vk::PipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable   {vk::False},
        .colorWriteMask{
            vk::ColorComponentFlagBits::eR | 
            vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | 
            vk::ColorComponentFlagBits::eA
        }
    };

    vk::PipelineColorBlendStateCreateInfo colorBlending {
        .logicOpEnable  {vk::False},
        .logicOp        {vk::LogicOp::eCopy},
        .attachmentCount{1},
        .pAttachments   {&colorBlendAttachment}
    };

    // Pipeline layout
    vk::PushConstantRange pushConstantRange{
        .stageFlags{vk::ShaderStageFlagBits::eFragment},
        .offset    {0},
        .size      {sizeof(CoordinateChanges)}
    };

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
        .setLayoutCount        {0},
        .pushConstantRangeCount{1},
        .pPushConstantRanges   {&pushConstantRange}
    };
    
    m_pipelineLayout = vk::raii::PipelineLayout(m_device, pipelineLayoutInfo);

    // Dynamic rendering & pipeline create info
    vk::StructureChain<
        vk::GraphicsPipelineCreateInfo,
        vk::PipelineRenderingCreateInfo
    >
    pipelineCreateInfoChain{
        {.stageCount        {numShaderStages},
        .pStages            {shaderStages.data()},
        .pVertexInputState  {&vertexInputInfo},
        .pInputAssemblyState{&inputAssembly},
        .pViewportState     {&viewportState},
        .pRasterizationState{&rasterizer},
        .pMultisampleState  {&multisampling},
        .pColorBlendState   {&colorBlending},
        .pDynamicState      {&dynamicState},
        .layout             {m_pipelineLayout},
        .renderPass         {nullptr}
        },

        {.colorAttachmentCount{1},
        .pColorAttachmentFormats{&m_swapChainSurfaceFormat.format}
        }
    };

    // finally create graphics pipeline
    m_graphicsPipeline = vk::raii::Pipeline(m_device, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
}

std::vector<char> App::readFile(const std::string &filename) {
    // start at end of file to get full file size
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Error: failed to open file");
    }

    // create buffer of the same size of the file & read from the beginning
    std::vector<char> buffer(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    file.close();
    return buffer;
}

vk::raii::ShaderModule App::createShaderModule(const std::vector<char> &code) const {
    vk::ShaderModuleCreateInfo shaderModuleCreateInfo{
        .codeSize{code.size() * sizeof(char)},
        .pCode   {reinterpret_cast<const uint32_t*>(code.data())}
    };
    vk::raii::ShaderModule shaderModule{m_device, shaderModuleCreateInfo};
    return shaderModule;
}

void App::createCommandPool() {
    vk::CommandPoolCreateInfo commandPoolCreateInfo{
        .flags{vk::CommandPoolCreateFlagBits::eResetCommandBuffer},
        .queueFamilyIndex{m_graphicsQueueFamilyIndex}
    };

    m_commandPool = vk::raii::CommandPool(m_device, commandPoolCreateInfo);
}

void App::createCommandBuffers() {
    vk::CommandBufferAllocateInfo commandBufferInfo{
        .commandPool       {m_commandPool},
        .level             {vk::CommandBufferLevel::ePrimary},
        .commandBufferCount{Config::maxFramesInFlight}
    };

    m_commandBuffers = vk::raii::CommandBuffers(m_device, commandBufferInfo);
}

void App::createSyncObjects() {
    for ([[maybe_unused]] size_t i : std::views::iota(0uz, m_swapChainImages.size())) {
        m_renderFinishedSemaphores.emplace_back(m_device, vk::SemaphoreCreateInfo());
    }

    for ([[maybe_unused]] size_t i : std::views::iota(0, Config::maxFramesInFlight)) {
        m_presentCompleteSemaphores.emplace_back(m_device, vk::SemaphoreCreateInfo());
        m_drawFences.emplace_back(m_device, vk::FenceCreateInfo{.flags{vk::FenceCreateFlagBits::eSignaled}});
    }
}

void App::recreateSwapchain() {
    // edge case: potentially 0-sized framebuffer
    int width{}, height{};
    glfwGetFramebufferSize(m_window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(m_window, &width, &height);
        glfwWaitEvents();
    }

    m_device.waitIdle();
    cleanupSwapchain();
    createSwapchain();
    createImageViews();  // NOTE: image views created were for that particular swapchain
}

void App::cleanupSwapchain() {
    m_swapChainImageViews.clear();
    m_swapChain = nullptr;
}

/* Render Loop */
void App::mainLoop() {
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        processUserInput();
        drawFrame();
        
        // Update delta time
        double currFrame{glfwGetTime()};
        m_deltaTime = currFrame - m_lastFrame;
        m_lastFrame = currFrame;
    }

    m_device.waitIdle();
}

void App::processUserInput() {
    // Escape button
    if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(m_window, true);
    }
    
    // Offsets
    const double moveSpeed{0.5 / m_zoom * m_deltaTime};
    if (glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS) {
        m_yOffset += moveSpeed;
    }

    if (glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS) {
        m_xOffset -= moveSpeed;
    }

    if (glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS) {
        m_yOffset -= moveSpeed;
    }

    if (glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS) {
        m_xOffset += moveSpeed;
    }

    // Zoom
    double zoomFactor{0.5 * m_deltaTime};
    if (glfwGetKey(m_window, GLFW_KEY_EQUAL) == GLFW_PRESS) {
        m_zoom += zoomFactor;
    }

    if (m_zoom > 0.1 && glfwGetKey(m_window, GLFW_KEY_MINUS) == GLFW_PRESS) {
        m_zoom -= zoomFactor;
    }
}

void App::recordCommandBuffer(uint32_t imageIndex) {
    auto &commandBuffer{m_commandBuffers[m_frameIndex]};
    commandBuffer.begin({});

    // transition image for rendering
    transitionImageLayout(
        imageIndex,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::AccessFlagBits2::eNone,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput
    );

    // set up color attachment
    vk::ClearValue clearColor{vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f)};
    vk::RenderingAttachmentInfo attachmentInfo{
        .imageView  {m_swapChainImageViews[imageIndex]},
        .imageLayout{vk::ImageLayout::eColorAttachmentOptimal},
        .loadOp     {vk::AttachmentLoadOp::eClear},
        .storeOp    {vk::AttachmentStoreOp::eStore},
        .clearValue {clearColor}
    };
    
    // set up rendering info
    vk::RenderingInfo renderingInfo{
        .renderArea{
            .offset{0, 0},
            .extent{m_swapChainExtent}
        },
        .layerCount          {1},
        .colorAttachmentCount{1},
        .pColorAttachments   {&attachmentInfo}
    };

    // begin rendering
    commandBuffer.beginRendering(renderingInfo);
    
    commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(m_swapChainExtent.width), static_cast<float>(m_swapChainExtent.height), 0.0f, 1.0f));
    commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), m_swapChainExtent));

    CoordinateChanges coordinateChanges{
        .xOffset{m_xOffset},
        .yOffset{m_yOffset},
        .zoom   {m_zoom}
    };
    commandBuffer.pushConstants<CoordinateChanges>(*m_pipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, coordinateChanges);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *m_graphicsPipeline);
    commandBuffer.draw(6, 1, 0, 0);

    // end rendering
    commandBuffer.endRendering();

    // transition image to present layout
    transitionImageLayout(
        imageIndex,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::AccessFlagBits2::eNone,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::PipelineStageFlagBits2::eBottomOfPipe
    );

    commandBuffer.end();
}

void App::transitionImageLayout(
    uint32_t imageIndex,
    vk::ImageLayout oldLayout,
    vk::ImageLayout newLayout,
    vk::AccessFlags2 srcAccessMask,
    vk::AccessFlags2 dstAccessMask,
    vk::PipelineStageFlags2 srcStageMask,
    vk::PipelineStageFlags2 dstStageMask
) {
    vk::ImageMemoryBarrier2 barrier{
        .srcStageMask       {srcStageMask},
        .srcAccessMask      {srcAccessMask},
        .dstStageMask       {dstStageMask},
        .dstAccessMask      {dstAccessMask},
        .oldLayout          {oldLayout},
        .newLayout          {newLayout},
        .srcQueueFamilyIndex{vk::QueueFamilyIgnored},
        .dstQueueFamilyIndex{vk::QueueFamilyIgnored},
        .image              {m_swapChainImages[imageIndex]},
        .subresourceRange{
            .aspectMask    {vk::ImageAspectFlagBits::eColor},
            .baseMipLevel  {0},
            .levelCount    {1},
            .baseArrayLayer{0},
            .layerCount    {1}
        }
    };

    vk::DependencyInfo dependencyInfo{
        .dependencyFlags        {},
        .imageMemoryBarrierCount{1},
        .pImageMemoryBarriers   {&barrier}
    };

    m_commandBuffers[m_frameIndex].pipelineBarrier2(dependencyInfo);
}

void App::drawFrame() {
    // 1. Wait for rendering to be finished from previous queue submission to this image
    vk::Result fenceResult{m_device.waitForFences(*m_drawFences[m_frameIndex], vk::True, UINT64_MAX)};
    if (fenceResult != vk::Result::eSuccess) {
        throw std::runtime_error("Error: failed to wait for Vulkan fence");
    }

    // 2. Grab image & record command buffer for it
    auto [result, imageIndex] = m_swapChain.acquireNextImage(UINT64_MAX, *m_presentCompleteSemaphores[m_frameIndex], nullptr);
    if (result == vk::Result::eErrorOutOfDateKHR || m_framebufferResized) {
        m_framebufferResized = false;
        recreateSwapchain();
        return;
    }

    // NOTE: only reset if we are submitting work, to prevent a potential deadlock!
    m_device.resetFences(*m_drawFences[m_frameIndex]);
    recordCommandBuffer(imageIndex);

    // 3. Submit command buffer
    vk::PipelineStageFlags waitDestinationStageMask{vk::PipelineStageFlagBits::eColorAttachmentOutput};
    vk::SubmitInfo submitInfo{
        .waitSemaphoreCount  {1},
        .pWaitSemaphores     {&(*m_presentCompleteSemaphores[m_frameIndex])},
        .pWaitDstStageMask   {&waitDestinationStageMask},
        .commandBufferCount  {1},
        .pCommandBuffers     {&(*m_commandBuffers[m_frameIndex])},
        .signalSemaphoreCount{1},
        .pSignalSemaphores   {&(*m_renderFinishedSemaphores[imageIndex])}
    };
    
    m_graphicsQueue.submit(submitInfo, *m_drawFences[m_frameIndex]);

    // 4. Present image
    const vk::PresentInfoKHR presentInfo{
        .waitSemaphoreCount{1},
        .pWaitSemaphores   {&(*m_renderFinishedSemaphores[imageIndex])},
        .swapchainCount    {1},
        .pSwapchains       {&(*m_swapChain)},
        .pImageIndices     {&imageIndex}
    };

    result = m_graphicsQueue.presentKHR(presentInfo);
    if (result == vk::Result::eErrorOutOfDateKHR || m_framebufferResized) {
        m_framebufferResized = false;
        recreateSwapchain();
    }

    m_frameIndex = (m_frameIndex + 1) % Config::maxFramesInFlight;
}

/* Cleanup */
void App::cleanup() {
    cleanupSwapchain();
    glfwDestroyWindow(m_window);
    glfwTerminate();
}
