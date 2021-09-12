#ifndef ENGINE_H
#define ENGINE_H
#include "window.h"
#include <vulkan.h>
#include <string_view>
#include <vector>

//może pogrupować dane w struktury?

class Engine
{
public:
    Engine();
    ~Engine();
    void run();
    void stop();

private:
    uint32_t mFramesInFlight = 2; //ile będzie jednocześnie command bufferów

    uint16_t mWindowWidth = 800;
    uint16_t mWindowHeight = 600;
    bool mRun = true;

    void assertVkSuccess(VkResult res, std::string_view);
    void createInstance();
    void createDevice();
    void createSurface();
    void createSwapchain();
    void createDepthImage();
    void createCommandBuffer();
    void createFence();
    void createSemaphores();
    void createRenderPass();
    void createFrameBuffer();
    void render(uint32_t i);

    uint32_t findMemoryProperties(uint32_t memoryTypeBitsRequirement, VkMemoryPropertyFlags requiredProperties);

    /*------- instance ------------*/
    VkInstance mInstance = VK_NULL_HANDLE;

    /*----- physical device -------*/
    VkPhysicalDeviceProperties mPhysicalDeviceProperties = {};
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties mDeviceMemoryProperties;

    /*--------- queues ------------*/
    VkQueueFamilyProperties mQueueFamilyProperties = {};
    uint32_t mQueueFamilyIndex = 0;
    uint32_t mQueueCount = 1;
    VkDevice mDevice = VK_NULL_HANDLE;
    VkQueue mQueue = VK_NULL_HANDLE;

    /*---- surface and window -----*/
    VkSurfaceKHR mSurface = VK_NULL_HANDLE;
    VkSurfaceCapabilitiesKHR mSurfaceCapabilities = {};
    Window mWindow;
    std::vector<VkSurfaceFormatKHR> mSurfaceFormats;

    /*- swapchain and image views -*/
    uint32_t mSwapchainImageCount = 0;
    VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;
    std::vector<VkImage> mSwapchainImages;
    VkFormat mSwapchainImageFormat;
    std::vector<VkImageView> mImageViews;

    /*------- depth image/view -----*/
    std::vector<VkImage> mDepthImages;
    std::vector<VkImageView> mDepthImageViews;
    std::vector<VkDeviceMemory> mDeviceMemory {};

    /*------- command buffer -------*/
    VkCommandPool mCommandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> mCommandBuffers = std::vector<VkCommandBuffer>(2);
    VkCommandBufferBeginInfo mCommandBufferBeginInfo = {};

    /*--- fences and semaphores ----*/
    std::vector<VkFence> mQueueSubmitFences;
    std::vector<VkSemaphore> mQueueSubmitSemaphores;
    std::vector<VkSemaphore> mAcquireSemaphores;

    /*--------- renderpass ---------*/
    VkRenderPass mRenderPass = VK_NULL_HANDLE;
    VkSubpassDescription mSubpass {};

    /*-------- framebuffer ---------*/
    std::vector<VkFramebuffer> mFramebuffers;
};

#endif // ENGINE_H
