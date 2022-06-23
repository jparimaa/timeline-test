#include <cstdio>
#include <vector>
#include <cstdlib>
#include <array>
#include <set>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct QueueFamilyIndices
{
    int graphicsFamily = -1;
    int computeFamily = -1;
    int presentFamily = -1;
};

VkInstance m_instance;
#ifdef _MSC_VER
VkDebugUtilsMessengerEXT m_debugMessenger;
#endif
GLFWwindow* m_window;
bool m_shouldQuit = false;
VkSurfaceKHR m_surface;
QueueFamilyIndices m_queueFamilyIndices;
VkRenderPass m_renderPass;
VkSwapchainKHR m_swapchain;
std::vector<VkImage> m_swapchainImages;
std::vector<VkImageView> m_swapchainImageViews;
std::vector<VkFramebuffer> m_framebuffers;
VkPhysicalDevice m_physicalDevice;
VkDevice m_device;
VkQueue m_graphicsQueue;
VkQueue m_presentQueue;
VkCommandPool m_graphicsCommandPool;
VkCommandBuffer m_commandBuffer;
std::vector<VkSemaphore> m_imageAvailableBinarySemaphores;
std::vector<VkSemaphore> m_renderFinishedBinarySemaphores;
std::vector<VkSemaphore> m_timelineSemaphores;
VkFence m_fence;

#define ENABLE_TIMELINE_SEMAPHORES

#ifdef _MSC_VER
const std::vector<const char*> c_validationLayers{"VK_LAYER_KHRONOS_validation"};
#else
const std::vector<const char*> c_validationLayers{};
#endif
const std::vector<const char*> c_instanceExtensions{VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
const std::vector<const char*> c_deviceExtensions{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME, //
#ifdef ENABLE_TIMELINE_SEMAPHORES
    VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME //
#endif
};
const VkPresentModeKHR c_presentMode = VK_PRESENT_MODE_FIFO_KHR;
const int c_windowWidth = 800;
const int c_windowHeight = 600;
const VkSurfaceFormatKHR c_windowSurfaceFormat{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
const VkExtent2D c_windowExtent{c_windowWidth, c_windowHeight};
const uint32_t c_swapchainImageCount = 3;
const uint64_t c_timeout = 10000000000;

#define VK_CHECK(f)                                                                             \
    do                                                                                          \
    {                                                                                           \
        const VkResult result = (f);                                                            \
        if (result != VK_SUCCESS)                                                               \
        {                                                                                       \
            printf("Abort. %s failed at %s:%d. Result = %d\n", #f, __FILE__, __LINE__, result); \
            abort();                                                                            \
        }                                                                                       \
    } while (false)

#define CHECK(f)                                                           \
    do                                                                     \
    {                                                                      \
        if (!(f))                                                          \
        {                                                                  \
            printf("Abort. %s failed at %s:%d\n", #f, __FILE__, __LINE__); \
            abort();                                                       \
        }                                                                  \
    } while (false)

template<typename T>
uint32_t ui32Size(const T& container)
{
    return static_cast<uint32_t>(container.size());
}

#ifdef _MSC_VER
VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                                                  VkDebugUtilsMessageTypeFlagsEXT message_type,
                                                  const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
                                                  void* user_data)
{
    if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        printf("Vulkan warning ");
    }
    else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        printf("Vulkan error ");
    }
    else
    {
        return VK_FALSE;
    }

    printf("(%d)\n%s\n%s\n\n", callback_data->messageIdNumber, callback_data->pMessageIdName, callback_data->pMessage);
    return VK_FALSE;
}
#endif

bool hasAllQueueFamilies(const QueueFamilyIndices& indices)
{
    return indices.graphicsFamily != -1 && indices.computeFamily != -1 && indices.presentFamily != -1;
}

void getQueueFamilies()
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, queueFamilies.data());

    QueueFamilyIndices indices;
    for (unsigned int i = 0; i < queueFamilies.size(); ++i)
    {
        if (queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = i;
        }

        if (queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            indices.computeFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, m_surface, &presentSupport);
        if (queueFamilies[i].queueCount > 0 && presentSupport)
        {
            indices.presentFamily = i;
        }

        if (hasAllQueueFamilies(indices))
        {
            break;
        }
    }

    m_queueFamilyIndices = indices;
}

void glfwErrorCallback(int error, const char* description)
{
    printf("GLFW error %d: %s\n", error, description);
}

void initGLFW()
{
    glfwSetErrorCallback(glfwErrorCallback);
    const int glfwInitialized = glfwInit();
    CHECK(glfwInitialized == GLFW_TRUE);

    const int vulkanSupported = glfwVulkanSupported();
    CHECK(vulkanSupported == GLFW_TRUE);
}

void createInstance()
{
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "MyApp";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

#ifdef _MSC_VER
    VkDebugUtilsMessengerCreateInfoEXT debugUtilsCreateInfo{};
    debugUtilsCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugUtilsCreateInfo.messageSeverity = //
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | //
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    debugUtilsCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debugUtilsCreateInfo.pfnUserCallback = debugUtilsCallback;
#endif

    std::vector<const char*> instanceExtensions;
    unsigned int glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    for (unsigned int i = 0; i < glfwExtensionCount; ++i)
    {
        instanceExtensions.push_back(glfwExtensions[i]);
    }

    instanceExtensions.insert(instanceExtensions.end(), c_instanceExtensions.begin(), c_instanceExtensions.end());

    VkInstanceCreateInfo instanceCreateInfo{};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;
    instanceCreateInfo.enabledExtensionCount = ui32Size(instanceExtensions);
    instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
    instanceCreateInfo.enabledLayerCount = ui32Size(c_validationLayers);
    instanceCreateInfo.ppEnabledLayerNames = c_validationLayers.data();
#ifdef _MSC_VER
    instanceCreateInfo.pNext = &debugUtilsCreateInfo;
#endif

    VK_CHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance));

#ifdef _MSC_VER
    auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
    CHECK(vkCreateDebugUtilsMessengerEXT);
    VK_CHECK(vkCreateDebugUtilsMessengerEXT(m_instance, &debugUtilsCreateInfo, nullptr, &m_debugMessenger));
#endif
}

void handleKey(GLFWwindow* /*window*/, int key, int /*scancode*/, int action, int /*mods*/)
{
    if (action == GLFW_RELEASE && key == GLFW_KEY_ESCAPE)
    {
        m_shouldQuit = true;
    }
}

void createWindow()
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    m_window = glfwCreateWindow(c_windowWidth, c_windowHeight, "Vulkan", nullptr, nullptr);
    CHECK(m_window);
    glfwSetWindowPos(m_window, 200, 200);

    glfwSetKeyCallback(m_window, handleKey);

    VK_CHECK(glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface));
}

void getPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    CHECK(deviceCount);

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());
    m_physicalDevice = devices[0];
    CHECK(m_physicalDevice != VK_NULL_HANDLE);

    VkPhysicalDeviceProperties m_physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &m_physicalDeviceProperties);
    printf("GPU: %s\n", m_physicalDeviceProperties.deviceName);
}

void createDevice()
{
    const std::set<int> uniqueQueueFamilies = //
        {
            m_queueFamilyIndices.graphicsFamily,
            m_queueFamilyIndices.computeFamily,
            m_queueFamilyIndices.presentFamily //
        };

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    const float queuePriority = 1.0f;
    for (int queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    VkPhysicalDeviceVulkan12Features device12Features{};
    device12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
#ifdef ENABLE_TIMELINE_SEMAPHORES
    device12Features.timelineSemaphore = true;
#endif

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &device12Features;
    createInfo.queueCreateInfoCount = ui32Size(queueCreateInfos);
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = ui32Size(c_deviceExtensions);
    createInfo.ppEnabledExtensionNames = c_deviceExtensions.data();
    createInfo.enabledLayerCount = ui32Size(c_validationLayers);
    createInfo.ppEnabledLayerNames = c_validationLayers.data();

    VK_CHECK(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device));

    vkGetDeviceQueue(m_device, m_queueFamilyIndices.graphicsFamily, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, m_queueFamilyIndices.presentFamily, 0, &m_presentQueue);
}

void createRenderPass()
{
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = nullptr;

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = c_windowSurfaceFormat.format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    const std::vector<VkAttachmentDescription> attachments{colorAttachment};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = ui32Size(attachments);
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VK_CHECK(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass));
}

void createSwapchain()
{
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = c_swapchainImageCount;
    createInfo.imageFormat = c_windowSurfaceFormat.format;
    createInfo.imageColorSpace = c_windowSurfaceFormat.colorSpace;
    createInfo.imageExtent = c_windowExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices = nullptr;
    createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = c_presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain));

    uint32_t queriedImageCount;
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &queriedImageCount, nullptr);
    CHECK(queriedImageCount == c_swapchainImageCount);
    m_swapchainImages.resize(c_swapchainImageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &queriedImageCount, m_swapchainImages.data());
}

void createSwapchainImageViews()
{
    const VkImageSubresourceRange SubresourceRance{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    m_swapchainImageViews.resize(m_swapchainImages.size());
    for (size_t i = 0; i < m_swapchainImageViews.size(); ++i)
    {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = m_swapchainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = c_windowSurfaceFormat.format;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange = SubresourceRance;

        VK_CHECK(vkCreateImageView(m_device, &createInfo, nullptr, &m_swapchainImageViews[i]));
    }
}

void createFramebuffers()
{
    m_framebuffers.resize(m_swapchainImageViews.size());

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = m_renderPass;
    framebufferInfo.width = c_windowWidth;
    framebufferInfo.height = c_windowHeight;
    framebufferInfo.layers = 1;

    for (size_t i = 0; i < m_swapchainImageViews.size(); ++i)
    {
        const std::vector<VkImageView> attachments{m_swapchainImageViews[i]};
        framebufferInfo.attachmentCount = ui32Size(attachments);
        framebufferInfo.pAttachments = attachments.data();
        VK_CHECK(vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_framebuffers[i]));
    }
}

void createCommandPool()
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = m_queueFamilyIndices.graphicsFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VK_CHECK(vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_graphicsCommandPool));
}

void allocateCommandBuffer()
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_graphicsCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VK_CHECK(vkAllocateCommandBuffers(m_device, &allocInfo, &m_commandBuffer));
}

void createSemaphores()
{
    {
        m_imageAvailableBinarySemaphores.resize(c_swapchainImageCount);
        m_renderFinishedBinarySemaphores.resize(c_swapchainImageCount);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        for (size_t i = 0; i < c_swapchainImageCount; ++i)
        {
            VK_CHECK(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableBinarySemaphores[i]));
            VK_CHECK(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedBinarySemaphores[i]));
        }
    }

#ifdef ENABLE_TIMELINE_SEMAPHORES
    {
        m_timelineSemaphores.resize(c_swapchainImageCount);

        VkSemaphoreTypeCreateInfo timelineCreateInfo{};
        timelineCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        timelineCreateInfo.pNext = NULL;
        timelineCreateInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        timelineCreateInfo.initialValue = 0;

        VkSemaphoreCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        createInfo.pNext = &timelineCreateInfo;
        createInfo.flags = 0;

        for (size_t i = 0; i < c_swapchainImageCount; ++i)
        {
            VK_CHECK(vkCreateSemaphore(m_device, &createInfo, NULL, &m_timelineSemaphores[i]));
        }
    }
#endif
}

void createFence()
{
    VkFenceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    createInfo.pNext = nullptr;
    createInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    vkCreateFence(m_device, &createInfo, nullptr, &m_fence);
}

void destroyResources()
{
    vkDeviceWaitIdle(m_device);
    vkDestroyFence(m_device, m_fence, nullptr);
    for (size_t i = 0; i < c_swapchainImageCount; ++i)
    {
        vkDestroySemaphore(m_device, m_imageAvailableBinarySemaphores[i], nullptr);
        vkDestroySemaphore(m_device, m_renderFinishedBinarySemaphores[i], nullptr);
#ifdef ENABLE_TIMELINE_SEMAPHORES
        vkDestroySemaphore(m_device, m_timelineSemaphores[i], nullptr);
#endif
    }
    vkDestroyCommandPool(m_device, m_graphicsCommandPool, nullptr);

    for (const VkFramebuffer& framebuffer : m_framebuffers)
    {
        vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }

    for (const VkImageView& imageView : m_swapchainImageViews)
    {
        vkDestroyImageView(m_device, imageView, nullptr);
    }

    vkDestroyRenderPass(m_device, m_renderPass, nullptr);

    vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

    vkDestroyDevice(m_device, nullptr);

    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    glfwDestroyWindow(m_window);
    glfwTerminate();

#ifdef _MSC_VER
    auto vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
    CHECK(vkDestroyDebugUtilsMessengerEXT);
    vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
#endif
    vkDestroyInstance(m_instance, nullptr);
}

int main(void)
{
    initGLFW();
    createInstance();
    createWindow();
    getPhysicalDevice();
    getQueueFamilies();
    createDevice();
    createRenderPass();
    createSwapchain();
    createSwapchainImageViews();
    createFramebuffers();
    createCommandPool();
    allocateCommandBuffer();
    createSemaphores();
    createFence();

    uint64_t semaphoreCounter = 0;
    uint64_t frameIndex = 0;
    while (!(glfwWindowShouldClose(m_window) || m_shouldQuit))
    {
        uint32_t imageIndex;
        VK_CHECK(vkAcquireNextImageKHR(m_device, m_swapchain, c_timeout, m_imageAvailableBinarySemaphores[frameIndex], VK_NULL_HANDLE, &imageIndex));
        VK_CHECK(vkWaitForFences(m_device, 1, &m_fence, true, c_timeout));
        VK_CHECK(vkResetFences(m_device, 1, &m_fence));

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
        beginInfo.pInheritanceInfo = nullptr;

        VkCommandBuffer cb = m_commandBuffer;
        vkResetCommandBuffer(cb, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
        vkBeginCommandBuffer(cb, &beginInfo);

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {0.0f, 0.0f, 0.2f, 1.0f};
        clearValues[1].depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_renderPass;
        renderPassInfo.framebuffer = m_framebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = c_windowExtent;
        renderPassInfo.clearValueCount = ui32Size(clearValues);
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(cb, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdEndRenderPass(cb);

        VK_CHECK(vkEndCommandBuffer(cb));

        const std::vector<VkPipelineStageFlags> waitStages{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

        ++semaphoreCounter;
        const std::vector<uint64_t> signalValues(2, semaphoreCounter);
        void* next = nullptr;
        std::vector<VkSemaphore> signalSemaphores;

#ifdef ENABLE_TIMELINE_SEMAPHORES
        signalSemaphores.push_back(m_timelineSemaphores[frameIndex]);

        VkTimelineSemaphoreSubmitInfo timelineInfo{};
        timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        timelineInfo.pNext = NULL;
        timelineInfo.waitSemaphoreValueCount = 0;
        timelineInfo.pWaitSemaphoreValues = NULL;
        timelineInfo.signalSemaphoreValueCount = ui32Size(signalValues);
        timelineInfo.pSignalSemaphoreValues = signalValues.data();
        next = &timelineInfo;
#endif

        signalSemaphores.push_back(m_renderFinishedBinarySemaphores[frameIndex]);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = next;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &m_imageAvailableBinarySemaphores[frameIndex];
        submitInfo.pWaitDstStageMask = waitStages.data();
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_commandBuffer;
        submitInfo.signalSemaphoreCount = ui32Size(signalSemaphores);
        submitInfo.pSignalSemaphores = signalSemaphores.data();

        VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_fence));

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &m_renderFinishedBinarySemaphores[frameIndex];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &m_swapchain;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = nullptr;

        VK_CHECK(vkQueuePresentKHR(m_presentQueue, &presentInfo));

        glfwPollEvents();

        ++frameIndex;
        if (frameIndex == c_swapchainImageCount)
        {
            frameIndex = 0;
        }
    }

    destroyResources();

    return 0;
}