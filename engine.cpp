#include "engine.h"
#include <iostream>
#include <vector>
#include <cstring>

Engine::Engine() : mWindow(this)  //m_window musi być zdefiniowane tutaj bo podczas tworzenia Engine i ono powstanie i jest zależne od Engine
{
    createInstance();
    createDevice();
    createSurface();
    createSwapchain();
    createCommandBuffer();
    createFence();
    createSemaphore();
    createRenderPass();
    createFrameBuffer();
}

Engine::~Engine()
{
    vkDestroyFramebuffer(mDevice, mFramebuffer, NULL);
    vkDestroyRenderPass(mDevice, mRenderPass, NULL);
    vkDestroySemaphore(mDevice, mQueueSubmitSemaphore, NULL);
    vkDestroyFence(mDevice, mQueueSubmitFence, NULL);
    vkDestroyCommandPool(mDevice, mCommandPool, NULL);
    for(auto imageView : mImageViews) //nie trzeba & bo pod spodem wskaźniki - skopiowanie ich to nadal to samo miejsce w pamięci
    {
        vkDestroyImageView(mDevice, imageView, NULL);
    }
    vkDestroySwapchainKHR(mDevice, mSwapchain, NULL);
    vkDestroySurfaceKHR(mInstance, mSurface, NULL);
    vkDeviceWaitIdle(mDevice); //czeka aż device skończy wszystko robić, by móc go zniszczyć
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
        "VK_LAYER_LUNARG_standard_validation"
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

    VkInstanceCreateInfo instanceCreateInfo{};
    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pNext = NULL;
    instanceCreateInfo.flags = 0;
    instanceCreateInfo.pApplicationInfo = NULL;
    instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(requiredLayerNames.size());
    instanceCreateInfo.ppEnabledLayerNames = requiredLayerNames.data();
    instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensionNames.size());
    instanceCreateInfo.ppEnabledExtensionNames = requiredExtensionNames.data();

    res = vkCreateInstance(&instanceCreateInfo, NULL, &mInstance);
    assertVkSuccess(res, "failed to create instance");
}

uint32_t Engine::findMemoryProperties(VkPhysicalDeviceMemoryProperties *memoryProperties, uint32_t memoryTypeBitsRequirement, VkMemoryPropertyFlags requiredProperties)
{
    const uint32_t memoryCount = memoryProperties->memoryTypeCount;

    for(uint32_t memoryIndex = 0; memoryIndex < memoryCount; memoryIndex++)
    {
        const uint32_t memoryTypeBits = (1 << memoryIndex); //zapis??????
        const bool isRequiredMemoryType = memoryTypeBitsRequirement & memoryTypeBits;

        const VkMemoryPropertyFlags properties = memoryProperties->memoryTypes[memoryIndex].propertyFlags;
        const bool hasRequiredProperties = (properties && requiredProperties) == requiredProperties;

        if(isRequiredMemoryType && hasRequiredProperties)
        {
            return static_cast<uint32_t>(memoryIndex);
        }
    }

    throw std::runtime_error("failed to find memory properties");
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

    for(mQueueFamilyIndex = 0; mQueueFamilyIndex < queueFamilyProperties.size(); mQueueFamilyIndex++)  //lepiej wybrać .size() bo ograniczamy się do wektora, count niekoniecznie prawdziwe(?)
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

    res = vkEnumerateDeviceExtensionProperties(mPhysicalDevice, NULL, &propertyCount, NULL); //funkcja zwraca do propertycount liczbę extension
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

    //TODO: sprawdzić jak flagi zależą od typów karty graficznej
    uint32_t memoryTypeBitsRequirement = 1;
    std::vector<VkMemoryPropertyFlags> requiredProperties(2);

    if(mPhysicalDeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
    {
        memoryTypeBitsRequirement = 7;
    }

    vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &mDeviceMemoryProperties);
    findMemoryProperties(mDeviceMemoryProperties, memoryTypeBitsRequirement, );

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
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    VkWaylandSurfaceCreateInfoKHR waylandSurfaceCreateInfo {};
    waylandSurfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
    waylandSurfaceCreateInfo.pNext = NULL;
    waylandSurfaceCreateInfo.flags = 0;
    waylandSurfaceCreateInfo.display = mWindow.getDisplay();
    waylandSurfaceCreateInfo.surface = mWindow.getSurface(); //surface waylandowy, ten mSurface bedzie vulkanowy

    VkResult res = vkCreateWaylandSurfaceKHR(mInstance, &waylandSurfaceCreateInfo, NULL, &mSurface);
    assertVkSuccess(res, "failed to create wayland surface");

#endif
    res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &mSurfaceCapabilities);
    assertVkSuccess(res, "failed to get surface capabilities");

    uint32_t surfaceFormatCount = 0;
    res = vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &surfaceFormatCount, NULL);
    assertVkSuccess(res, "failed to get surface formats");

    mSurfaceFormats.resize(surfaceFormatCount); //bo zapamiętujemy surface_formats do swapchaina

    res = vkGetPhysicalDeviceSurfaceFormatsKHR(mPhysicalDevice, mSurface, &surfaceFormatCount, mSurfaceFormats.data());
    assertVkSuccess(res, "failed to get surface formats");

    VkBool32 supported;
    res = vkGetPhysicalDeviceSurfaceSupportKHR(mPhysicalDevice, mQueueFamilyIndex, mSurface, &supported);
    if(res == VK_FALSE)
    {
        std::runtime_error("surface is not supported");
    }
}

void Engine::createSwapchain()
{
    VkSurfaceCapabilitiesKHR surfaceCapabilities {};
    VkResult res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(mPhysicalDevice, mSurface, &surfaceCapabilities);
    assertVkSuccess(res, "failed to get surface capabilities");

    VkSwapchainCreateInfoKHR swapchainCreateInfo {};
    swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainCreateInfo.pNext = NULL;
    swapchainCreateInfo.flags = 0;
    swapchainCreateInfo.surface = mSurface;
    swapchainCreateInfo.minImageCount = (mSurfaceCapabilities.minImageCount + 1) < mSurfaceCapabilities.maxImageCount ? (mSurfaceCapabilities.minImageCount + 1) : mSurfaceCapabilities.minImageCount; // warunek ? jeśli true : jeśli false
    swapchainCreateInfo.imageFormat = mSurfaceFormats[0].format;
    swapchainCreateInfo.imageColorSpace = mSurfaceFormats[0].colorSpace;
    swapchainCreateInfo.imageExtent = surfaceCapabilities.currentExtent;
    swapchainCreateInfo.imageArrayLayers = 1; //non-stereoscopic 3d app = 1
    swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; //dostęp do obrazka będzie mieć jednocześnie jedna rodzina kolejek
    swapchainCreateInfo.queueFamilyIndexCount = mQueueCount;
    swapchainCreateInfo.pQueueFamilyIndices = VK_NULL_HANDLE; //zero bo SHARING_MODE_EXCLUSIVE
    swapchainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR; //brak transformacji
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;                         // SPRÓBOWAĆ INNĄ ALPHĘ
    swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; //v-sync
    swapchainCreateInfo.clipped = VK_TRUE;
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

    mSwapchainImageFormat = swapchainCreateInfo.imageFormat;

    res = vkCreateSwapchainKHR(mDevice, &swapchainCreateInfo, NULL, &mSwapchain);
    assertVkSuccess(res, "failed to create swapchain");

    uint32_t swapchainImageCount = 0;

    res = vkGetSwapchainImagesKHR(mDevice, mSwapchain, &swapchainImageCount, NULL);
    assertVkSuccess(res, "failed to get swapchain images");
    mSwapchainImages.resize(swapchainImageCount);
    res = vkGetSwapchainImagesKHR(mDevice, mSwapchain, &swapchainImageCount, mSwapchainImages.data()); //swapchain sam tworzy swoje image - nie potrzebuję do nich imagecreateinfo
    assertVkSuccess(res, "failed to get swapchain images");

    mImageViews.resize(mSwapchainImages.size()); //tyle samo imageView co obrazków swapchain

    for(uint32_t i = 0; i < mImageViews.size(); i++)
    {
        mImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        mImageViewCreateInfo.pNext = NULL;
        mImageViewCreateInfo.flags = 0;
        mImageViewCreateInfo.image = mSwapchainImages[i];
        mImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        mImageViewCreateInfo.format = mSwapchainImageFormat;
        mImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        mImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        mImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        mImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        mImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        mImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
        mImageViewCreateInfo.subresourceRange.levelCount = 1;
        mImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        mImageViewCreateInfo.subresourceRange.layerCount = 1;

        VkResult res = vkCreateImageView(mDevice, &mImageViewCreateInfo, NULL, &mImageViews[i]);
        assertVkSuccess(res, "failed to create image view");
    }
}

void Engine::createDepthImage()
{
    VkImageCreateInfo depthImageCreateInfo {};
    depthImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthImageCreateInfo.pNext = NULL;
    depthImageCreateInfo.flags = 0;
    depthImageCreateInfo.imageType;
    depthImageCreateInfo.format;
    depthImageCreateInfo.extent;
    depthImageCreateInfo.mipLevels;
    depthImageCreateInfo.arrayLayers;
    depthImageCreateInfo.samples;
    depthImageCreateInfo.tiling;
    depthImageCreateInfo.usage;
    depthImageCreateInfo.sharingMode;
    depthImageCreateInfo.queueFamilyIndexCount = mQueueCount;
    depthImageCreateInfo.pQueueFamilyIndices;
    depthImageCreateInfo.initialLayout;

    VkResult res = vkCreateImage(mDevice, &depthImageCreateInfo, NULL, &mDepthImage);
}

void Engine::createCommandBuffer()
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
    mCommandBufferBeginInfo.pInheritanceInfo = 0; // nie ma znaczenia dla primary command buffer
}

void Engine::createFence()
{
    VkFenceCreateInfo fenceCreateInfo {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.pNext = NULL;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; //tworzymy zasygnalizowany, żeby wykonało się render za pierwszym razem

    VkResult res = vkCreateFence(mDevice, &fenceCreateInfo, NULL, &mQueueSubmitFence);
    assertVkSuccess(res, "failed to create fence");
}

void Engine::createSemaphore()
{
    VkSemaphoreCreateInfo semaphoreCreateInfo {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = NULL;
    semaphoreCreateInfo.flags = 0;

    VkResult res = vkCreateSemaphore(mDevice, &semaphoreCreateInfo, NULL, &mQueueSubmitSemaphore);
    assertVkSuccess(res, "failed to create semaphore");
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
    attachmentDescriptions[0].format = mSwapchainImageFormat;               // !!!!!!!!!!!!!!!!!!!!!!!
    attachmentDescriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    attachmentDescriptions[1].flags = 0; //indeks 1 - depthattachment
    attachmentDescriptions[1].format = VK_FORMAT_D32_SFLOAT;
    attachmentDescriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachmentDescriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachmentDescriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachmentDescriptions[1].finalLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkRenderPassCreateInfo renderPassCreateInfo {}; // jeśli będę zapisywać globalnie TO USUNĄC NOWĄ DEKLARACJĘ
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
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

void Engine::createFrameBuffer()
{
    VkFramebufferCreateInfo framebufferCreateInfo {};
    framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferCreateInfo.pNext = NULL;
    framebufferCreateInfo.flags = 0;
    framebufferCreateInfo.renderPass = mRenderPass;
    framebufferCreateInfo.attachmentCount;
    framebufferCreateInfo.pAttachments;
    framebufferCreateInfo.width;
    framebufferCreateInfo.height;
    framebufferCreateInfo.layers;

    VkResult res = vkCreateFramebuffer(mDevice, &framebufferCreateInfo, NULL, &mFramebuffer);
    assertVkSuccess(res, "failed to create framebuffer");
}

void Engine::render(uint32_t imageIndex)
{
    vkWaitForFences(mDevice, 1, &mQueueSubmitFence, VK_TRUE, 0);
    //wait for fences zanim kolejny raz będę submitować queue - musi się skończyć poprzednie, potem go zresetujemy
    VkResult res = vkResetFences(mDevice, 1, &mQueueSubmitFence);
    assertVkSuccess(res, "failed to reset fence");

    /*-------- Begin Command Buffer ----------*/
    res = vkBeginCommandBuffer(mCommandBuffers[imageIndex], &mCommandBufferBeginInfo); //recording state
    assertVkSuccess(res, "failed to begin command buffers");

    VkClearColorValue clearColorValue;
    clearColorValue.float32[0] = 1.0f;
    clearColorValue.float32[1] = 0.0f;
    clearColorValue.float32[2] = 0.0f;
    clearColorValue.float32[3] = 1.0f;

    vkCmdClearColorImage(mCommandBuffers[imageIndex], mSwapchainImages[imageIndex], VK_IMAGE_LAYOUT_GENERAL, &clearColorValue, 1, &mImageViewCreateInfo.subresourceRange);

    res = vkEndCommandBuffer(mCommandBuffers[imageIndex]);
    assertVkSuccess(res, "failed to end command buffers");
    /*--------- End Command Buffer ----------*/

    VkSubmitInfo submitInfo {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = NULL;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = NULL;
    submitInfo.pWaitDstStageMask = NULL;
    submitInfo.commandBufferCount = 1; // jeden bo jednocześnie chcę wysłać 1 cmdbuff
    submitInfo.pCommandBuffers = &mCommandBuffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    // liczba semaforów, która będzie sygnalizowała że command buffer się wykonał
    submitInfo.pSignalSemaphores = &mQueueSubmitSemaphore;
    // wskaźnik na ten semafor

    res = vkQueueSubmit(mQueue, 1, &submitInfo, mQueueSubmitFence);
    assertVkSuccess(res, "failed to queue submit");

    VkPresentInfoKHR presentInfo {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = NULL;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &mQueueSubmitSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &mSwapchain;
    presentInfo.pImageIndices = &imageIndex;
    presentInfo.pResults = NULL;

    res = vkQueuePresentKHR(mQueue, &presentInfo);
    assertVkSuccess(res, "failed to queue presentation");
}

void Engine::run()
{
    uint32_t imageIndex = 1;

    while(mRun)
    {
        mWindow.handleEvents();
        render((imageIndex % mFramesInFlight));
        imageIndex++;
    }
}

void Engine::stop()
{
    mRun = false;
}
