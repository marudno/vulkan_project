#include "engine.h"
#include <fstream>
#include <array>
#include <iostream>
#include <vector>
#include <cstring>

Engine::Engine() : mWindow(this, mWindowWidth, mWindowHeight)
{
    createInstance();
    createDevice();
    createSurface();
    createSwapchain();
    createDepthImage();
    createCommandBuffer();
    createFence();
    createSemaphores();
    createRenderPass();
    createFrameBuffer();
    createPipeline();
}

Engine::~Engine()
{
    vkDeviceWaitIdle(mDevice);

    vkDestroyPipeline(mDevice, mPipeline, NULL);
    vkDestroyPipelineLayout(mDevice, mPipelineLayout, NULL);
    for(auto framebuffer : mFramebuffers)
    {
        vkDestroyFramebuffer(mDevice, framebuffer, NULL);
    }
    vkDestroyRenderPass(mDevice, mRenderPass, NULL);
    for(auto queueSubmitSemaphore : mQueueSubmitSemaphores)
    {
        vkDestroySemaphore(mDevice, queueSubmitSemaphore, NULL);
    }
    for(auto acquireSemaphore : mAcquireSemaphores)
    {
        vkDestroySemaphore(mDevice, acquireSemaphore, NULL);
    }
    for(auto queueSubmitFence : mQueueSubmitFences)
    {
        vkDestroyFence(mDevice, queueSubmitFence, NULL);
    }
    vkDestroyCommandPool(mDevice, mCommandPool, NULL);
    for(auto deviceMemory : mDeviceMemory)
    {
        vkFreeMemory(mDevice, deviceMemory, NULL);
    }
    for(auto depthImageView : mDepthImageViews)
    {
        vkDestroyImageView(mDevice, depthImageView, NULL);
    }
    for(auto depthImage : mDepthImages)
    {
        vkDestroyImage(mDevice, depthImage, NULL);
    }
    for(auto imageView : mImageViews)
    {
        vkDestroyImageView(mDevice, imageView, NULL);
    }
    vkDestroySwapchainKHR(mDevice, mSwapchain, NULL);
    vkDestroySurfaceKHR(mInstance, mSurface, NULL);
    vkDestroyDevice(mDevice, NULL);
    vkDestroyInstance(mInstance, NULL);
}


void Engine::assertVkSuccess(VkResult res, std::string_view errorMsg)
{
    if(res != VK_SUCCESS)
    {
        throw std::runtime_error(errorMsg.data());
    }
}

void Engine::createInstance()
{
    const std::vector<const char*> requiredLayerNames =
    {
        "VK_LAYER_KHRONOS_validation"
    };

    const std::vector<const char*> requiredExtensionNames =
    {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_PLATFORM_SURFACE_EXTENSION_NAME
    };

    VkResult res;
    uint32_t propertyCount = 0;
    res = vkEnumerateInstanceLayerProperties(&propertyCount, NULL);
    assertVkSuccess(res, "failed to enumerate instance properties");
    std::vector<VkLayerProperties> layerProperties(propertyCount);
    res = vkEnumerateInstanceLayerProperties(&propertyCount, layerProperties.data());
    assertVkSuccess(res, "failed to enumerate instance properties");

    for(const auto& layerName : requiredLayerNames)
    {
        bool found = false;
        for(const auto& lp : layerProperties)
        {
            if(std::strcmp(lp.layerName, layerName) == 0)
            {
                found = true;
                break;
            }
        }
        if(!found)
        {
            throw std::runtime_error("required layer not supported");
        }
    }

    res = vkEnumerateInstanceExtensionProperties(NULL, &propertyCount, NULL);
    assertVkSuccess(res, "failed to enumerate extension properties");
    std::vector<VkExtensionProperties> extensionProperties(propertyCount);
    res = vkEnumerateInstanceExtensionProperties(NULL, &propertyCount, extensionProperties.data());
    assertVkSuccess(res, "failed to enumerate extension properties");

    for(const auto& extensionName : requiredExtensionNames)
    {
        bool found = false;
        for(const auto& ep : extensionProperties)
        {
            if(std::strcmp(ep.extensionName, extensionName) == 0)
            {
                found = true;
                break;
            }
        }
        if(!found)
        {
            throw std::runtime_error("required extension not supported");
        }
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = NULL;
    appInfo.pApplicationName = "App Name";
    appInfo.applicationVersion = VK_MAKE_VERSION(0,1,0);
    appInfo.pEngineName = "Engine Name";
    appInfo.engineVersion = VK_MAKE_VERSION(0,1,0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo instanceCreateInfo{};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pNext = NULL;
    instanceCreateInfo.flags = 0;
    instanceCreateInfo.pApplicationInfo = &appInfo;
    instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(requiredLayerNames.size());
    instanceCreateInfo.ppEnabledLayerNames = requiredLayerNames.data();
    instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensionNames.size());
    instanceCreateInfo.ppEnabledExtensionNames = requiredExtensionNames.data();

    res = vkCreateInstance(&instanceCreateInfo, NULL, &mInstance);
    assertVkSuccess(res, "failed to create instance");
}

uint32_t Engine::findMemoryProperties(uint32_t memoryTypeBitsRequirement, VkMemoryPropertyFlags requiredProperties)
{
    const uint32_t memoryCount = mDeviceMemoryProperties.memoryTypeCount;

    for(uint32_t memoryIndex = 0; memoryIndex < memoryCount; memoryIndex++)
    {
        const uint32_t memoryTypeBits = (1 << memoryIndex);
        const bool isRequiredMemoryType = memoryTypeBitsRequirement & memoryTypeBits;

        if(isRequiredMemoryType)
        {
            const VkMemoryPropertyFlags properties = mDeviceMemoryProperties.memoryTypes[memoryIndex].propertyFlags;
            const bool hasRequiredProperties = (properties & requiredProperties) == requiredProperties;

            if(isRequiredMemoryType && hasRequiredProperties)
            {
                return memoryIndex;
            }
        }
    }

    throw std::runtime_error("failed to find memory type");
}

void Engine::createDevice()
{
    VkResult res;
    uint32_t physicalDeviceCount;

    res = vkEnumeratePhysicalDevices(mInstance, &physicalDeviceCount, NULL);
    assertVkSuccess(res, "failed to enumerate physical devices");
    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);

    res = vkEnumeratePhysicalDevices(mInstance, &physicalDeviceCount, physicalDevices.data());
    assertVkSuccess(res, "failed to enumerate physical devices");

    bool found = false;

    for(const auto& pd : physicalDevices)
    {
        vkGetPhysicalDeviceProperties(pd, &mPhysicalDeviceProperties);
        if(mPhysicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            mPhysicalDevice = pd;
            found = true;
            break;
        }
    }

    if(!found)
    {
        for(const auto& pd : physicalDevices)
        {
            vkGetPhysicalDeviceProperties(pd, &mPhysicalDeviceProperties);
            if(mPhysicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
            {
                mPhysicalDevice = pd;
                found = true;
                break;
            }
        }
    }

    if(!found)
    {
        throw std::runtime_error("failed to find physical device");
    }

    uint32_t queueFamilyPropertyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyPropertyCount, NULL);
    std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(mPhysicalDevice, &queueFamilyPropertyCount, queueFamilyProperties.data());

    for(mQueueFamilyIndex = 0; mQueueFamilyIndex < queueFamilyProperties.size(); mQueueFamilyIndex++)
    {
        if((queueFamilyProperties[mQueueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT)
        {
            mQueueFamilyProperties = queueFamilyProperties[mQueueFamilyIndex];
            break;
        }
    }

    float queuePriority = 1.0;

    VkDeviceQueueCreateInfo deviceQueueCreateInfo {};
    deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    deviceQueueCreateInfo.pNext = NULL;
    deviceQueueCreateInfo.flags = 0;
    deviceQueueCreateInfo.queueFamilyIndex = mQueueFamilyIndex;
    deviceQueueCreateInfo.queueCount = mQueueCount;
    deviceQueueCreateInfo.pQueuePriorities = &queuePriority;

    uint32_t propertyCount = 0;

    res = vkEnumerateDeviceExtensionProperties(mPhysicalDevice, NULL, &propertyCount, NULL);
    assertVkSuccess(res, "failed to enumerate device properties");
    std::vector<VkExtensionProperties> extensionProperties(propertyCount);
    res = vkEnumerateDeviceExtensionProperties(mPhysicalDevice, NULL, &propertyCount, extensionProperties.data());
    assertVkSuccess(res, "failed to enumerate device properties");

    const std::vector<const char*> requiredExtensionNames =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    for(const auto& extensionName : requiredExtensionNames)
    {
        bool found = false;
        for(const auto& ep : extensionProperties)
        {
            if(std::strcmp(ep.extensionName, extensionName) == 0)
            {
                found = true;
                break;
            }
        }
        if(!found)
        {
            throw std::runtime_error("required device extension not supported");
        }
    }

    VkPhysicalDeviceFeatures physicalDeviceFeatures;
    vkGetPhysicalDeviceFeatures(mPhysicalDevice, &physicalDeviceFeatures);

    VkDeviceCreateInfo deviceCreateInfo {};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = NULL;
    deviceCreateInfo.flags = 0;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.ppEnabledLayerNames = NULL;
    deviceCreateInfo.enabledExtensionCount = requiredExtensionNames.size();
    deviceCreateInfo.ppEnabledExtensionNames = requiredExtensionNames.data();
    deviceCreateInfo.pEnabledFeatures = NULL;  //na razie nie włączam żadnego, jak będą potrzebne to tu wrócę

    res = vkCreateDevice(mPhysicalDevice, &deviceCreateInfo, NULL, &mDevice);
    assertVkSuccess(res,"failed to create device");

    vkGetDeviceQueue(mDevice, mQueueFamilyIndex, 0, &mQueue);
}

void Engine::createSurface()
{
#ifdef VK_USE_PLATFORM_WIN32_KHR
    VkWin32SurfaceCreateInfoKHR win32SurfaceCreateInfo {};
    win32SurfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    win32SurfaceCreateInfo.pNext = NULL;
    win32SurfaceCreateInfo.flags = 0;
    win32SurfaceCreateInfo.hinstance = mWindow.getHinstance();
    win32SurfaceCreateInfo.hwnd = mWindow.getHwnd();

    VkResult res = vkCreateWin32SurfaceKHR(mInstance, &win32SurfaceCreateInfo, NULL, &mSurface);
    assertVkSuccess(res, "failed to create win32 surface");
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    VkXcbSurfaceCreateInfoKHR xcbSurfaceCreateInfo;
    xcbSurfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    xcbSurfaceCreateInfo.pNext = NULL;
    xcbSurfaceCreateInfo.flags = 0;
    xcbSurfaceCreateInfo.connection = mWindow.mConnection;
    xcbSurfaceCreateInfo.window = mWindow.mWindowId;

    VkResult res = vkCreateXcbSurfaceKHR(mInstance, &xcbSurfaceCreateInfo, NULL, &mSurface);
    assertVkSuccess(res, "failed to create xcb surface");
#endif
    res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &mSurfaceCapabilities);
    assertVkSuccess(res, "failed to get surface capabilities");

    uint32_t surfaceFormatCount = 0;
    res = vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &surfaceFormatCount, NULL);
    assertVkSuccess(res, "failed to get surface formats");

    mSurfaceFormats.resize(surfaceFormatCount);

    res = vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &surfaceFormatCount, mSurfaceFormats.data());
    assertVkSuccess(res, "failed to get surface formats");

    VkBool32 supported;
    res = vkGetPhysicalDeviceSurfaceSupportKHR(mPhysicalDevice, mQueueFamilyIndex, mSurface, &supported);
    if(res == VK_FALSE)
    {
        std::runtime_error("surface is not supported");
    }
}

void Engine::createSwapchain() // tworzę zqwsze kiedy zmieniają sięjego parametry
{
    VkSurfaceCapabilitiesKHR surfaceCapabilities {};
    VkResult res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &surfaceCapabilities); //pozbyc sie tego ew. surface ze swapchainem
    assertVkSuccess(res, "failed to get surface capabilities");

    VkSwapchainCreateInfoKHR swapchainCreateInfo {};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.pNext = NULL;
    swapchainCreateInfo.flags = 0;
    swapchainCreateInfo.surface = mSurface;
    swapchainCreateInfo.minImageCount = (mSurfaceCapabilities.minImageCount + 1) < mSurfaceCapabilities.maxImageCount ? (mSurfaceCapabilities.minImageCount + 1) : mSurfaceCapabilities.minImageCount; // warunek ? jeśli true : jeśli false
    swapchainCreateInfo.imageFormat = mSurfaceFormats[0].format;
    swapchainCreateInfo.imageColorSpace = mSurfaceFormats[0].colorSpace;
    swapchainCreateInfo.imageArrayLayers = 1; //non-stereoscopic 3d app = 1
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; //dostęp do obrazka będzie mieć jednocześnie jedna rodzina kolejek
    swapchainCreateInfo.queueFamilyIndexCount = mQueueCount;
    swapchainCreateInfo.pQueueFamilyIndices = VK_NULL_HANDLE; //zero bo ^SHARING_MODE_EXCLUSIVE
    swapchainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR; //brak transformacji
    //TODO: sprobowac inna alphe
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // v-sync, aktualizuję cały bufor i dopiero go wyświetlam
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    if((mSurfaceCapabilities.currentExtent.width != 0xffffffff) && (mSurfaceCapabilities.currentExtent.height != 0xffffffff))
    {
        swapchainCreateInfo.imageExtent = surfaceCapabilities.currentExtent;
    }
    else
    {
        swapchainCreateInfo.imageExtent.width = mWindowWidth;
        swapchainCreateInfo.imageExtent.height = mWindowHeight;
    }
    mSwapchainWidth = swapchainCreateInfo.imageExtent.width;
    mSwapchainHeight = swapchainCreateInfo.imageExtent.height;

    mSwapchainImageFormat = swapchainCreateInfo.imageFormat;

    res = vkCreateSwapchainKHR(mDevice, &swapchainCreateInfo, NULL, &mSwapchain);
    assertVkSuccess(res, "failed to create swapchain");

    res = vkGetSwapchainImagesKHR(mDevice, mSwapchain, &mSwapchainImageCount, NULL);
    assertVkSuccess(res, "failed to get swapchain images");
    mSwapchainImages.resize(mSwapchainImageCount);
    res = vkGetSwapchainImagesKHR(mDevice, mSwapchain, &mSwapchainImageCount, mSwapchainImages.data());
    assertVkSuccess(res, "failed to get swapchain images");

    mImageViews.resize(mSwapchainImages.size());

    for(uint32_t i = 0; i < mImageViews.size(); i++)
    {
        VkImageViewCreateInfo imageViewCreateInfo {};
        imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.pNext = NULL;
        imageViewCreateInfo.flags = 0;
        imageViewCreateInfo.image = mSwapchainImages[i];
        imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format = mSwapchainImageFormat;
        imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        imageViewCreateInfo.subresourceRange.levelCount = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount = 1;

        VkResult res = vkCreateImageView(mDevice, &imageViewCreateInfo, NULL, &mImageViews[i]);
        assertVkSuccess(res, "failed to create image view");
    }
}

void Engine::createDepthImage()
{
    VkImage depthImage = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;

    mDepthImages.resize(mSwapchainImageCount);
    mDepthImageViews.resize(mSwapchainImageCount);

    VkImageCreateInfo depthImageCreateInfo {};
    depthImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthImageCreateInfo.pNext = NULL;
    depthImageCreateInfo.flags = 0;
    depthImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    depthImageCreateInfo.format = VK_FORMAT_D32_SFLOAT;
    depthImageCreateInfo.extent.width = mWindowWidth;
    depthImageCreateInfo.extent.height = mWindowHeight;
    depthImageCreateInfo.extent.depth = 1;
    depthImageCreateInfo.mipLevels = 1;
    depthImageCreateInfo.arrayLayers = 1;
    depthImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthImageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    depthImageCreateInfo.queueFamilyIndexCount = 0;
//    depthImageCreateInfo.pQueueFamilyIndices; ignored
    depthImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImageViewCreateInfo depthImageViewCreateInfo {};
    depthImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthImageViewCreateInfo.pNext = NULL;
    depthImageViewCreateInfo.flags = 0;
    depthImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthImageViewCreateInfo.format = depthImageCreateInfo.format;
    depthImageViewCreateInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    depthImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    depthImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    depthImageViewCreateInfo.subresourceRange.layerCount = 1;
    depthImageViewCreateInfo.subresourceRange.levelCount = 1;

    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &mDeviceMemoryProperties); //TODO przenieść to w mądrzejsze miejsce

    mDeviceMemory.resize(mSwapchainImageCount);

    for(uint32_t i = 0; i < mSwapchainImageCount; i++)
    {
        VkResult res = vkCreateImage(mDevice, &depthImageCreateInfo, NULL, &depthImage); // każdy image musi mieć podpiętą pamięć zaalokowaną na karcie -> findMemoryProperties
        assertVkSuccess(res, "failed to create depth image");
        mDepthImages[i] = depthImage;

        depthImageViewCreateInfo.image = depthImage;

        const auto requiredProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        VkMemoryRequirements memoryRequirements;
        vkGetImageMemoryRequirements(mDevice, depthImage, &memoryRequirements);

        uint32_t memoryIndex = findMemoryProperties(memoryRequirements.memoryTypeBits, requiredProperties);

        VkMemoryAllocateInfo allocateCreateInfo {};
        allocateCreateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateCreateInfo.pNext = NULL;
        allocateCreateInfo.allocationSize = memoryRequirements.size;
        allocateCreateInfo.memoryTypeIndex = memoryIndex;

        vkAllocateMemory(mDevice, &allocateCreateInfo, NULL, &mDeviceMemory[i]);
        vkBindImageMemory(mDevice, depthImage, mDeviceMemory[i], 0);

        res = vkCreateImageView(mDevice, &depthImageViewCreateInfo, NULL, &depthImageView);
        assertVkSuccess(res, "failed to create depth image view");
        mDepthImageViews[i] = depthImageView;
    }
}

void Engine::createCommandBuffer() // trzeba się synchronizować, żeby procesor nie zaczął nagrywać komend zamin nie skończy ich wykonywać
{
    VkCommandPoolCreateInfo commandPoolCreateInfo = {};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.pNext = NULL;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolCreateInfo.queueFamilyIndex = mQueueFamilyIndex;

    VkResult res = vkCreateCommandPool(mDevice, &commandPoolCreateInfo, NULL, &mCommandPool);
    assertVkSuccess(res, "failed to create command pool");

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.pNext = NULL;
    commandBufferAllocateInfo.commandPool = mCommandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = mFramesInFlight;

    res = vkAllocateCommandBuffers(mDevice, &commandBufferAllocateInfo, mCommandBuffers.data());  //initial state
    assertVkSuccess(res, "failed to allocate command buffers");

    mCommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    mCommandBufferBeginInfo.pNext = NULL;
    mCommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    mCommandBufferBeginInfo.pInheritanceInfo = 0;
}

void Engine::createFence()
{
    VkFenceCreateInfo fenceCreateInfo {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.pNext = NULL;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; //tworzymy zasygnalizowany, żeby wykonało się render za pierwszym razem

    VkFence queueSubmitFence = VK_NULL_HANDLE;
    mQueueSubmitFences.resize(mFramesInFlight);

    for(uint32_t i = 0; i < mFramesInFlight; i++)
    {
        VkResult res = vkCreateFence(mDevice, &fenceCreateInfo, NULL, &queueSubmitFence);
        assertVkSuccess(res, "failed to create fence");
        mQueueSubmitFences[i] = queueSubmitFence;
    }
}

void Engine::createSemaphores()
{
    VkSemaphoreCreateInfo semaphoreCreateInfo {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = NULL;
    semaphoreCreateInfo.flags = 0;

    VkSemaphore queueSubmitSemaphore = VK_NULL_HANDLE;
    mQueueSubmitSemaphores.resize(mFramesInFlight);
    for(uint32_t i = 0; i < mFramesInFlight; i++)
    {
        VkResult res = vkCreateSemaphore(mDevice, &semaphoreCreateInfo, NULL, &queueSubmitSemaphore);
        assertVkSuccess(res, "failed to create queue submit semaphore");
        mQueueSubmitSemaphores[i] = queueSubmitSemaphore;
    }

    VkSemaphore acquireSemaphore = VK_NULL_HANDLE;
    mAcquireSemaphores.resize(mFramesInFlight);
    for(uint32_t i = 0; i < mFramesInFlight; i++)
    {
        VkResult res = vkCreateSemaphore(mDevice, &semaphoreCreateInfo, NULL, &acquireSemaphore);
        assertVkSuccess(res, "failed to create aqcuire semaphore");
        mAcquireSemaphores[i] = acquireSemaphore;
    }
}

void Engine::createRenderPass()
{
    VkAttachmentReference colorAttachment;
    colorAttachment.attachment = 0;
    colorAttachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachment;
    depthAttachment.attachment = 1;
    depthAttachment.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    mSubpass.flags = 0;
    mSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    mSubpass.inputAttachmentCount = 0;
    mSubpass.pInputAttachments = NULL;
    mSubpass.colorAttachmentCount = 1;
    mSubpass.pColorAttachments = &colorAttachment;
    mSubpass.pResolveAttachments = NULL;
    mSubpass.pDepthStencilAttachment = &depthAttachment;
    mSubpass.preserveAttachmentCount = 0;
    mSubpass.pPreserveAttachments = NULL;

    std::vector<VkAttachmentDescription> attachmentDescriptions(2);

    attachmentDescriptions[0].flags = 0; //indeks 0 - colorattachment
    attachmentDescriptions[0].format = mSwapchainImageFormat;
    attachmentDescriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    attachmentDescriptions[1].flags = 0; //indeks 1 - depthattachment
    attachmentDescriptions[1].format = VK_FORMAT_D32_SFLOAT;
    attachmentDescriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachmentDescriptions[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkRenderPassCreateInfo renderPassCreateInfo {};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.pNext = NULL;
    renderPassCreateInfo.flags = 0;
    renderPassCreateInfo.attachmentCount = 2; //color and depth
    renderPassCreateInfo.pAttachments = attachmentDescriptions.data();
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &mSubpass;
    renderPassCreateInfo.dependencyCount = 0;
    renderPassCreateInfo.pDependencies = NULL;

    VkResult res = vkCreateRenderPass(mDevice, &renderPassCreateInfo, NULL, &mRenderPass);
    assertVkSuccess(res, "failed to create renderpass");
}

void Engine::createFrameBuffer() //dla róznych obrazków rózny frame buffer
{
    for(uint32_t i = 0; i < mSwapchainImageCount; i++)
    {
        VkFramebuffer framebuffer = VK_NULL_HANDLE;

        std::vector<VkImageView> framebufferAttachment(mSwapchainImageCount);
        framebufferAttachment[0] = mImageViews[i]; //już konkretne, w renderpass tylko szablon
        framebufferAttachment[1] = mDepthImageViews[i];

        VkFramebufferCreateInfo framebufferCreateInfo {};
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.pNext = NULL;
        framebufferCreateInfo.flags = 0;
        framebufferCreateInfo.renderPass = mRenderPass;
        framebufferCreateInfo.attachmentCount = 2; //color and depth
        framebufferCreateInfo.pAttachments = framebufferAttachment.data();
        framebufferCreateInfo.width = mWindowWidth;
        framebufferCreateInfo.height = mWindowHeight;
        framebufferCreateInfo.layers = 1;
        // dla każdego swapchain jeden framebuff i potem podczas renderowania jak sprawdzę na którym obrazku mogę pisać to na podstawie tego wybieram framebuff o danym indeksie

        VkResult res = vkCreateFramebuffer(mDevice, &framebufferCreateInfo, NULL, &framebuffer);
        assertVkSuccess(res, "failed to create framebuffer");
        mFramebuffers.push_back(framebuffer);
    }
}

void Engine::createPipeline()
{
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pNext = NULL;
    pipelineLayoutCreateInfo.flags = 0;
    pipelineLayoutCreateInfo.setLayoutCount = 0;
    pipelineLayoutCreateInfo.pSetLayouts = VK_NULL_HANDLE; //pewnie kiedyś będę chciała to ustawić
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0; // 256bajtów do szybciej aktualizacji
    pipelineLayoutCreateInfo.pPushConstantRanges = VK_NULL_HANDLE;

    VkResult res = vkCreatePipelineLayout(mDevice, &pipelineLayoutCreateInfo, NULL, &mPipelineLayout);
    assertVkSuccess(res, "failed to create pipeline layout");

    std::vector<VkShaderModuleCreateInfo> shaderModuleCreateInfos(2);
    std::vector<VkPipelineShaderStageCreateInfo> shaderStageCreateInfos(2);

    /*----------- open vertex shader --------*/
    std::ifstream vsfile("shaders/vs.spv", std::ios::ate | std::ios::binary);
    if(!vsfile)
    {
        throw std::runtime_error("shader file does not exist");
    }
    const size_t vsSize = vsfile.tellg();
    if (vsSize == 0)
    {
        throw std::runtime_error("shader file empty");
    }
    vsfile.seekg(0);
    std::vector<uint32_t> vsCode((vsSize - 1) / 4 + 1);
    vsfile.read(reinterpret_cast<char*>(vsCode.data()), vsSize);

    shaderModuleCreateInfos[0].sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; //reprezentuje skompilowany shader
    shaderModuleCreateInfos[0].pNext = NULL;
    shaderModuleCreateInfos[0].flags = 0;
    shaderModuleCreateInfos[0].codeSize = vsSize;
    shaderModuleCreateInfos[0].pCode = vsCode.data();
    VkShaderModule vertexShaderModule;
    res = vkCreateShaderModule(mDevice, &shaderModuleCreateInfos[0], NULL, &vertexShaderModule);
    assertVkSuccess(res, "failed to create vs module");

    shaderStageCreateInfos[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; //reprezentuje cały etap
    shaderStageCreateInfos[0].pNext = NULL;
    shaderStageCreateInfos[0].flags = 0;
    shaderStageCreateInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStageCreateInfos[0].module = vertexShaderModule;
    shaderStageCreateInfos[0].pName = "main";
    shaderStageCreateInfos[0].pSpecializationInfo = NULL;

    /*----------- open fragment shader --------*/
    std::ifstream fsfile("shaders/fs.spv", std::ios::ate | std::ios::binary);
    if(!fsfile)
    {
        throw std::runtime_error("shader file does not exist");
    }
    const size_t fsSize = fsfile.tellg();
    if (fsSize == 0)
    {
        throw std::runtime_error("shader file empty");
    }
    fsfile.seekg(0);
    std::vector<uint32_t> fsCode((fsSize - 1) / 4 + 1);
    fsfile.read(reinterpret_cast<char*>(fsCode.data()), fsSize);

    shaderModuleCreateInfos[1].sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderModuleCreateInfos[1].pNext = NULL;
    shaderModuleCreateInfos[1].flags = 0;
    shaderModuleCreateInfos[1].codeSize = fsSize;
    shaderModuleCreateInfos[1].pCode = fsCode.data();
    VkShaderModule fragmentShaderModule;
    res = vkCreateShaderModule(mDevice, &shaderModuleCreateInfos[1], NULL, &fragmentShaderModule);
    assertVkSuccess(res, "failed to create fs module");

    shaderStageCreateInfos[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageCreateInfos[1].pNext = NULL;
    shaderStageCreateInfos[1].flags = 0;
    shaderStageCreateInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStageCreateInfos[1].module = fragmentShaderModule;
    shaderStageCreateInfos[1].pName = "main";
    shaderStageCreateInfos[1].pSpecializationInfo = NULL;

    struct alignas(16) Vertex
    {
        std::array<float, 3> position;
        float pad;
        std::array<float, 4> color;
    };

    VkVertexInputBindingDescription bindingDescription {};
    bindingDescription.binding = 0; //indeks vertexbuff z którego będą pobierane atrybuty
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; //indeksuję vertexbuff indeksem wierzchołków
    //binding = 1, przykładowo vertex input instance - gdy do rysowania kilka elementów

    std::vector<VkVertexInputAttributeDescription> vertexAttributesDescriptions(2);
    vertexAttributesDescriptions[0].location = 0;  //vertex position
    vertexAttributesDescriptions[0].binding = 0;
    vertexAttributesDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexAttributesDescriptions[0].offset = 0;

    vertexAttributesDescriptions[1].location = 1;  //vertex color
    vertexAttributesDescriptions[1].binding = 0;
    vertexAttributesDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    vertexAttributesDescriptions[1].offset = 16; //odległość Vertex::color od początku struktury czyli rozmiar Vertex::position

    VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo{};
    vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputCreateInfo.pNext = NULL;
    vertexInputCreateInfo.flags = 0;
    vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
    vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputCreateInfo.vertexAttributeDescriptionCount = vertexAttributesDescriptions.size(); //vertex color and position
    vertexInputCreateInfo.pVertexAttributeDescriptions = vertexAttributesDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo{};
    inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyCreateInfo.pNext = NULL;
    inputAssemblyCreateInfo.flags = 0;
    inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0;
    viewport.y = mSwapchainHeight;
    viewport.width = mSwapchainWidth;
    viewport.height = -static_cast<float>(mSwapchainHeight);
    viewport.minDepth = 0;
    viewport.maxDepth = 1;

    VkRect2D scissor{};
    scissor.extent = {mSwapchainWidth, mSwapchainHeight};
    scissor.offset = {0, 0};

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.pNext = NULL;
    viewportStateCreateInfo.flags = 0;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.pViewports = &viewport;
    viewportStateCreateInfo.scissorCount = 1;
    viewportStateCreateInfo.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo{};
    rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationStateCreateInfo.pNext = NULL;
    rasterizationStateCreateInfo.flags = 0;
    rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
    rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE; // wyłączam, że jeżeli trójkąty są z tyłu to ich nie rysuję
    rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE; //nie ma znaczenia bo cullmode = off
    rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
    rasterizationStateCreateInfo.depthBiasConstantFactor = 0;
    rasterizationStateCreateInfo.depthBiasClamp = 0;
    rasterizationStateCreateInfo.depthBiasSlopeFactor = 0;
    rasterizationStateCreateInfo.lineWidth = 1;

    VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo{};
    multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleStateCreateInfo.pNext = NULL;
    multisampleStateCreateInfo.flags = 0;
    multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE; // czy fragment shader jest odpalany dla sampli
    multisampleStateCreateInfo.minSampleShading = 0;
    multisampleStateCreateInfo.pSampleMask = NULL;
    multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
    multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo{};
    depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilStateCreateInfo.pNext = NULL;
    depthStencilStateCreateInfo.flags = 0;
    depthStencilStateCreateInfo.depthTestEnable = VK_TRUE; // czy sprawdzamy dla każdego pixela głębię i ją porównujemy
    depthStencilStateCreateInfo.depthWriteEnable = VK_TRUE; // jeśli przejdzie test to czy zapisujemy
    depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
    depthStencilStateCreateInfo.front = {};
    depthStencilStateCreateInfo.back = {};
    depthStencilStateCreateInfo.minDepthBounds = 0;
    depthStencilStateCreateInfo.maxDepthBounds = 0;

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState{};
    colorBlendAttachmentState.blendEnable = VK_FALSE;
    colorBlendAttachmentState.srcColorBlendFactor = {};
    colorBlendAttachmentState.dstColorBlendFactor = {};
    colorBlendAttachmentState.colorBlendOp = {};
    colorBlendAttachmentState.srcAlphaBlendFactor = {};
    colorBlendAttachmentState.dstAlphaBlendFactor = {};
    colorBlendAttachmentState.alphaBlendOp = {};
    colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo{};
    colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendStateCreateInfo.pNext = NULL;
    colorBlendStateCreateInfo.flags = 0;
    colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
    colorBlendStateCreateInfo.logicOp = {};
    colorBlendStateCreateInfo.attachmentCount = 1;
    colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;
//    colorBlendStateCreateInfo.blendConstants;

    VkGraphicsPipelineCreateInfo pipelineCreateInfo;
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.pNext = NULL;
    pipelineCreateInfo.flags = 0;
    pipelineCreateInfo.stageCount = shaderStageCreateInfos.size();
    pipelineCreateInfo.pStages = shaderStageCreateInfos.data();
    pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
    pipelineCreateInfo.pTessellationState = NULL;
    pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
    pipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
    pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
    pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
    pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
    pipelineCreateInfo.pDynamicState = NULL;
    pipelineCreateInfo.layout = mPipelineLayout;
    pipelineCreateInfo.renderPass = mRenderPass;
    pipelineCreateInfo.subpass = 0;
    pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCreateInfo.basePipelineIndex = -1;

    res = vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, NULL, &mPipeline);
    vkDestroyShaderModule(mDevice, vertexShaderModule, NULL); //niszczę bo niepotrzebne
    vkDestroyShaderModule(mDevice, fragmentShaderModule, NULL);
    assertVkSuccess(res, "failed to create pipeline");
}

void Engine::render(uint32_t frameIndex)
{
    vkWaitForFences(mDevice, 1, &mQueueSubmitFences[frameIndex], VK_TRUE, 0);
    VkResult res = vkResetFences(mDevice, 1, &mQueueSubmitFences[frameIndex]);
    assertVkSuccess(res, "failed to reset fence");

    VkCommandBuffer cmdBuff = mCommandBuffers[frameIndex];
    uint32_t currentSwapchainImageIndex = 0;

    res = vkAcquireNextImageKHR(mDevice, mSwapchain, UINT64_MAX, mAcquireSemaphores[frameIndex], VK_NULL_HANDLE, &currentSwapchainImageIndex); //semafor azasygnalizowany kiedy obrazek będzie dostępny do rysowania
    assertVkSuccess(res, "failed to get current swapchain image index");

    /*-------- Begin Command Buffer ----------*/
    res = vkBeginCommandBuffer(cmdBuff, &mCommandBufferBeginInfo); //recording state
    assertVkSuccess(res, "failed to begin command buffers");

    VkClearColorValue clearColorValue;
    clearColorValue.float32[0] = 0.0f;
    clearColorValue.float32[1] = 0.5f;
    clearColorValue.float32[2] = 0.5f;
    clearColorValue.float32[3] = 0.0f;

    VkClearDepthStencilValue clearDepthValue;
    clearDepthValue.depth = 1.0f;
    //clearDepthValue.stencil; ignored - no stencil

    std::vector<VkClearValue> clearValues(2);
    clearValues[0].color = clearColorValue;
    clearValues[1].depthStencil = clearDepthValue;

    VkRenderPassBeginInfo renderPassBeginInfo {};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.pNext = NULL;
    renderPassBeginInfo.renderPass = mRenderPass;
    renderPassBeginInfo.framebuffer = mFramebuffers[currentSwapchainImageIndex];
    renderPassBeginInfo.renderArea.offset = {0,0};
    renderPassBeginInfo.renderArea.extent = {mWindowWidth, mWindowHeight};
    renderPassBeginInfo.clearValueCount = clearValues.size();
    renderPassBeginInfo.pClearValues = clearValues.data();

    /*----------- Begin RenderPass ----------*/
    vkCmdBeginRenderPass(cmdBuff, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    /*------------ End RenderPass -----------*/
    vkCmdEndRenderPass(cmdBuff);

    res = vkEndCommandBuffer(cmdBuff);
    assertVkSuccess(res, "failed to end command buffers");
    /*--------- End Command Buffer ----------*/

    VkPipelineStageFlags pipelineStageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;  // to jest po to, że jak semafor nie jest zasygnalizowany to nie pisze do obrazka, bo ten jeszcze
    // nie jest na to gotowy, optymalizacja, żeby karta mogła wykonwyać dziąłania na przód

    VkSubmitInfo submitInfo {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = NULL;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &mAcquireSemaphores[frameIndex]; //przekazuje semafor na ktory ma zaczekac karta
    submitInfo.pWaitDstStageMask = &pipelineStageFlags; // podajemy faze wykonania pipelinu na ktorym karta ma zaczekac na semafor
    submitInfo.commandBufferCount = 1; // jeden bo jednocześnie chcę wysłać 1 cmdbuff
    submitInfo.pCommandBuffers = &cmdBuff;
    submitInfo.signalSemaphoreCount = 1; // liczba semaforów, która będzie sygnalizowała że command buffer się wykonał
    submitInfo.pSignalSemaphores = &mQueueSubmitSemaphores[frameIndex]; // wskaźnik na ten semafor

    res = vkQueueSubmit(mQueue, 1, &submitInfo, mQueueSubmitFences[frameIndex]); //fence zasygnalizowany gdy koemndy na karcie zostaną wykonane
    assertVkSuccess(res, "failed to queue submit");

    VkPresentInfoKHR presentInfo {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = NULL;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &mQueueSubmitSemaphores[frameIndex];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &mSwapchain;
    presentInfo.pImageIndices = &currentSwapchainImageIndex;
    presentInfo.pResults = NULL;

    res = vkQueuePresentKHR(mQueue, &presentInfo);
    assertVkSuccess(res, "failed to queue presentation");
}

void Engine::run()
{
    uint32_t imageIndex = 1;

    while(true)
    {
        mWindow.handleEvents();
        if(mRun == false)
        {
            break;
        }
        render((imageIndex % mFramesInFlight));
        imageIndex++;
    }
}

void Engine::stop()
{
    mRun = false;
}
