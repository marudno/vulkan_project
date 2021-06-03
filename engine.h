#ifndef ENGINE_H
#define ENGINE_H
#include "window.h" //jeśli zaincludowane w pluku .h to nie ma potrzeby w pliku .cpp
#include <vulkan.h>
#include <string_view>
#include <vector>



class Engine
{
public:
    Engine();
    ~Engine();
    void run(); //bo aplikacja chodzi w pętli, która będzie się znajdować w tej funkcji
    void stop();

private:
    void assertVkSuccess(VkResult res, std::string_view);
    void createInstance();
    void createDevice();
    void createSurface();
    void createSwapchain();
    void createDepthImage();
    void createCommandBuffer();
    void createFence();
    void createSemaphore();
    void createRenderPass();
    void createFrameBuffer();
    void render(uint32_t i);

    bool mRun = true;

    /*------- instance ------------*/
    VkInstance mInstance = VK_NULL_HANDLE;

    /*----- physical device -------*/
    VkPhysicalDeviceProperties mPhysicalDeviceProperties = {};
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;

    /*--------- queues ------------*/
    VkQueueFamilyProperties mQueueFamilyProperties = {};
    uint32_t mQueueFamilyIndex = 0;
    uint32_t mQueueCount = 1;
    VkDevice mDevice = VK_NULL_HANDLE;
    VkQueue mQueue = VK_NULL_HANDLE;

    /*---- surface and window -----*/
    VkSurfaceKHR mSurface; //czy powinno być nullhadle?
    VkSurfaceCapabilitiesKHR mSurfaceCapabilities = {};
    Window mWindow;
    std::vector<VkSurfaceFormatKHR> mSurfaceFormats;

    /*- swapchain and image views -*/
    VkSwapchainKHR mSwapchain = VK_NULL_HANDLE;
    std::vector<VkImage> mSwapchainImages;
    VkFormat mSwapchainImageFormat;
    VkImageViewCreateInfo mImageViewCreateInfo;
    std::vector<VkImageView> mImageViews;

    /*------- command buffer -------*/
    VkCommandPool mCommandPool;
    uint32_t mFramesInFlight = 2; //ile będzie jednocześnie command bufferów
    std::vector<VkCommandBuffer> mCommandBuffers = std::vector<VkCommandBuffer>(2);
    VkCommandBufferBeginInfo mCommandBufferBeginInfo = {};

    /*--- fences and semaphores ----*/
    VkFence mQueueSubmitFence;
    VkSemaphore mQueueSubmitSemaphore;

    /*--------- renderpass ---------*/
    VkRenderPass mRenderPass;
    VkSubpassDescription mSubpass {};

    /*-------- framebuffer ---------*/
    VkFramebuffer mFramebuffer;
};

#endif // ENGINE_H
