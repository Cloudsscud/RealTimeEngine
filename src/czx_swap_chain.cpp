#include "czx_utils.h"
#include "czx_swap_chain.h"

// std
#include <cassert>
#include <array>
#include <cstdlib>
#include <cstring> 
#include <iostream>  
#include <limits>
#include <set>
#include <stdexcept> 

namespace czx {

    // 构造函数会初始化交换链及其依赖资源，准备进入渲染流程。
    CzxSwapChain::CzxSwapChain(CzxDevice& deviceRef, VkExtent2D extent)  // 通过设备引用和当前窗口尺寸构造交换链对象。
        : m_device{ deviceRef }, m_swapChainExtent{ extent } {  // 把设备对象和窗口尺寸保存为成员变量，后续创建交换链时直接使用。
        init();  // 调用统一初始化流程，把交换链、视图、渲染通道和同步对象一次性创建好。
    }

    // 初始化函数按顺序创建交换链、图像视图、渲染通道、深度资源和同步对象。
    void CzxSwapChain::init() {  // 这是统一的初始化入口，把交换链链路的所有依赖资源一次性准备好。
        createSwapChain();  // 初始化底层交换链对象，确定图像格式、尺寸和呈现模式。
        createDepthResources();  // 为深度测试准备对应的深度图像和视图。
        createSyncObjects();  // 创建信号量和围栏，用于帧间同步和图像可用状态控制。
    }


    // 析构函数负责销毁交换链相关的图像视图、帧缓冲、渲染通道和同步对象。
    CzxSwapChain::~CzxSwapChain() {
        for (auto imageView : m_imageViews) {  // 遍历所有交换链图像视图，逐个销毁。
            vkDestroyImageView(m_device.device(), imageView, nullptr);  // 释放每个图像视图占用的Vulkan对象
        }
        m_imageViews.clear();
        printf("Image views destroyed\n");

        // depth贴图自行析构

        if (m_swapChain != nullptr) {  // 只有当交换链对象确实存在时才销毁它。
            vkDestroySwapchainKHR(m_device.device(), m_swapChain, nullptr);
            m_swapChain = nullptr;
            printf("SwapChain destroyed\n");
        }

        // cleanup synchronization objects
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {  // 遍历每一帧的同步对象，依次释放。
            vkDestroySemaphore(m_device.device(), m_renderFinishedSemaphores[i], nullptr);  // 渲染完成信号量。
            vkDestroySemaphore(m_device.device(), m_presentFinishedSemaphores[i], nullptr);  // 图像可用信号量。
            vkDestroyFence(m_device.device(), m_inFlightFences[i], nullptr);  // 销毁帧围栏，用于同步提交队列。
        }
        printf("Semaphores and Fences destroyed\n");
    }

    // 从交换链中获取一个可用于渲染的图像索引，并等待其可用
    VkResult CzxSwapChain::acquireNextImage(uint32_t* imageIndex) {
        vkWaitForFences(
            m_device.device(),
            1,  // 只等待一个围栏对象
            &m_inFlightFences[m_currentFrame],  // 等待当前帧使用的图像对应的围栏，确保准备使用的图像已完全空闲
            VK_TRUE,  // 使用阻塞等待，直到围栏被触发
            std::numeric_limits<uint64_t>::max());  // 设置等待上限为最大值，表示一直等到可用

        VkResult result = vkAcquireNextImageKHR(
            m_device.device(),
            m_swapChain,
            std::numeric_limits<uint64_t>::max(),  // 设置无限等待时间，直到有可用图像
            m_presentFinishedSemaphores[m_currentFrame],  // 标识当前帧图像准备好的信号量，等待图像准备好后唤醒该信号量
            VK_NULL_HANDLE,  // 不绑定额外的信号量或Fence
            imageIndex);  // 输出获取到的图像索引

        return result;  // 返回Vulkan获取图像的结果码，便于上层判断是否成功或需要重建交换链。
    }

    // 将命令缓冲区提交给图像队列渲染并提交呈现请求，让渲染结果显示到窗口中
    VkResult CzxSwapChain::submitCommandBuffers(
        const VkCommandBuffer* buffers, uint32_t* imageIndex) {

        if (m_imagesInFlight[*imageIndex] != VK_NULL_HANDLE) {  // 如果当前图像原本对应的飞行帧忙碌中，需要先等待帧空闲
            vkWaitForFences(m_device.device(), 1, &m_imagesInFlight[*imageIndex], VK_TRUE, UINT64_MAX);
        }
        m_imagesInFlight[*imageIndex] = m_inFlightFences[m_currentFrame];  // 为该图像重新绑定当前飞行帧的栅栏

        VkPipelineStageFlags waitFlags = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

        VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 1, // 只有一个等待信号量
            .pWaitSemaphores = &m_presentFinishedSemaphores[m_currentFrame],    // 提交之前需要等待当前帧使用的图像呈现完毕，随后才可以提交队列用于渲染
            .pWaitDstStageMask = &waitFlags, // 在颜色附件输出之前等待信号量
            .commandBufferCount = 1,
            .pCommandBuffers = buffers,
            .signalSemaphoreCount =1,
            .pSignalSemaphores = &m_renderFinishedSemaphores[m_currentFrame]    // 提交完毕后唤醒当前帧的图像的渲染完成信号量，等待用于呈现
        };

        vkResetFences(m_device.device(), 1, &m_inFlightFences[m_currentFrame]);  // 重置当前帧围栏，避免重建交换链导致死锁

        VkResult result = vkQueueSubmit(m_device.graphicsQueue(), 1, &submitInfo, m_inFlightFences[m_currentFrame]);    //  正式提交到图形队列渲染，并阻塞当前飞行帧以防帧同步问题
        CHECK_VK_RESULT(result, "submit draw command buffer\n");

        // 渲染完毕后开始呈现
        VkPresentInfoKHR presentInfo = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &m_renderFinishedSemaphores[m_currentFrame], // 提交之前需要等待当前图像渲染完毕
            .swapchainCount = 1,
            .pSwapchains = &m_swapChain,    // 呈现图像使用的交换链
            .pImageIndices = imageIndex,    // 需要用于显示的交换链图像索引
        };

        result = vkQueuePresentKHR(m_device.presentQueue(), &presentInfo);  // 把呈现配置提交到呈现队列
        // 交换链可能由于窗口大小改变而过时，返回VK_ERROR_OUT_OF_DATE_KHR

        m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;  // 切换到下一帧，循环复用同步对象

        return result;  // 返回呈现结果
    }

    // 创建底层交换链对象，并根据窗口和物理设备能力选择合适的格式、模式和尺寸。
    void CzxSwapChain::createSwapChain() {  // 这个函数从设备能力和窗口信息中选出最合适的交换链参数并创建交换链。

        const VkSurfaceCapabilitiesKHR& surfaceCaps = m_device.physicalDevice().Selected().m_surfaceCaps;

        uint32_t imageCount = chooseImageCount(surfaceCaps);

        m_swapChainSurfaceFormat = chooseSwapSurfaceFormatAndColorSpace(m_device.physicalDevice().Selected().m_surfaceFormats);  // 选择像素的颜色格式
        m_swapChainPresentMode = chooseSwapPresentMode(m_device.physicalDevice().Selected().m_presentModes);  // 选择呈现模式,决定何时将图像呈现到屏幕上

        std::vector<uint32_t> queueFamilyindices = { m_device.graphicsQueueFamily()};

        //交换链创建信息结构体
        VkSwapchainCreateInfoKHR swapChainCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .surface = m_device.surface(),  // 绑定到特定的逻辑设备和窗口
            .minImageCount = imageCount,    // 交换链至少包含多少张图像
            .imageFormat = m_swapChainSurfaceFormat.format,    // 图像像素格式   // 这些属性要与surface保持一致
            .imageColorSpace = m_swapChainSurfaceFormat.colorSpace,    // 颜色空间
            .imageExtent = m_swapChainExtent,   // 图像尺寸
            .imageArrayLayers = 1,  // 仅适用于立体显示表面，此处不用
            .imageUsage = (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT), // 标记如何使用图像:作为颜色附件用于渲染，作为传输目标用于后处理
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,  // 独占共享模式(一般最佳性能)：一次只有一个队列族使用该图像资源
            .queueFamilyIndexCount = 1, // 队列族只适用于非独占模式
            .pQueueFamilyIndices = queueFamilyindices.data(),
            .preTransform = surfaceCaps.currentTransform,   // 当前surface的变换作为交换链图像的预变换方式，默认不变换
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,    // alpha混合设为不透明
            .presentMode = m_swapChainPresentMode,
            .clipped = VK_TRUE  // 启用裁剪
        };

        VkResult result = vkCreateSwapchainKHR(m_device.device(), &swapChainCreateInfo, nullptr, &m_swapChain);
        CHECK_VK_RESULT(result, "create swap chain!\n");
        printf("Swap chain created\n");

        uint32_t swapChainImageCount = 0;
        result = vkGetSwapchainImagesKHR(m_device.device(), m_swapChain, &swapChainImageCount, nullptr);  // 先查询实际创建出来的图像数量
        CHECK_VK_RESULT(result, "vkGetSwapchainImagesKHR (1)\n");
        assert(swapChainImageCount == imageCount && "swapchain and surface image count not same\n");

        printf("image count %d\n", swapChainImageCount);

        // 根据实际数量调整容器大小
        m_images.resize(swapChainImageCount);
        m_imageViews.resize(swapChainImageCount);

        result = vkGetSwapchainImagesKHR(m_device.device(), m_swapChain, &swapChainImageCount, m_images.data());  // 把交换链中所有图像句柄读出来保存
        CHECK_VK_RESULT(result, "vkGetSwapchainImagesKHR (2)\n");

        // 创建交换链图像视图
        int layerCount = 1;
        int mipLevels = 1;
        for (uint32_t i = 0; i < swapChainImageCount; ++i) {
            m_imageViews[i] = m_device.createImageView(m_images[i], m_swapChainSurfaceFormat.format,
                VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D, layerCount, mipLevels);
        }
    }

    // 创建深度缓冲资源
    void CzxSwapChain::createDepthResources() {
        VkFormat depthFormat = m_device.physicalDevice().Selected().m_depthFormat;  // 当前设备支持的深度格式
        VkExtent2D swapChainExtent = getSwapChainExtent();  // 获取当前交换链尺寸，深度图像需要与之一致

        m_depthTextures.reserve(imageCount());

        for (int i = 0; i < imageCount(); i++) {
            m_depthTextures.emplace_back(m_device);
            VkImageUsageFlags usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            VkMemoryPropertyFlags memProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;// 指定使用设备本地内存，适合高频渲染附件

            m_device.createImage(m_swapChainExtent.width, m_swapChainExtent.height, depthFormat, usage,
                memProps, m_depthTextures[i].m_image, m_depthTextures[i].m_imageMemory);

            VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkImageLayout newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            m_device.transitionImageLayout(m_depthTextures[i].m_image, oldLayout, newLayout);

            m_depthTextures[i].m_imageView = m_device.createImageView(m_depthTextures[i].m_image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
        }
    }

    // 创建信号量和围栏，用于控制图像可用、渲染完成以及帧间同步。
    void CzxSwapChain::createSyncObjects() {  // 这个函数准备每帧渲染时使用的同步对象
        m_presentFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);  // 按最大帧数准备图像可用信号量数组
        m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);  // 按最大帧数准备渲染完成信号量数组
        m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);  // 按最大帧数准备围栏数组
        m_imagesInFlight.resize(imageCount(), VK_NULL_HANDLE);  // 按交换链图像数量准备每张图像的占用记录

        VkSemaphoreCreateInfo semaphoreInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0
        };

        VkFenceCreateInfo fenceInfo = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT   // 初始化时让围栏处于已触发状态，避免第一次提交时卡住
        };

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {  // 遍历每一帧，创建对应的同步对象。
            if (vkCreateSemaphore(m_device.device(), &semaphoreInfo, nullptr, &m_presentFinishedSemaphores[i]) !=
                VK_SUCCESS ||
                vkCreateSemaphore(m_device.device(), &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) !=
                VK_SUCCESS ||
                vkCreateFence(m_device.device(), &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {  // 同时创建图像可用信号量、渲染完成信号量和围栏。
                throw std::runtime_error("failed to create synchronization objects for a frame!");  // 任一对象创建失败时抛出异常。
            }
        }
    }

    uint32_t CzxSwapChain::chooseImageCount(const VkSurfaceCapabilitiesKHR& surfaceCaps) {
        uint32_t requiredImageCount = surfaceCaps.minImageCount + 1;  // 至少多申请一张图像，避免前后帧竞争
        int finalImageCount = 0;

        if (surfaceCaps.maxImageCount > 0 &&
            requiredImageCount > surfaceCaps.maxImageCount) {  // 如果超过最大值，就截断到最大值。
            finalImageCount = surfaceCaps.maxImageCount;
        }
        else{
            finalImageCount = requiredImageCount;
        }
        return finalImageCount;
    }

    // 从可用格式中选择一个最适合当前窗口和显示器的颜色格式
    VkSurfaceFormatKHR CzxSwapChain::chooseSwapSurfaceFormatAndColorSpace(const std::vector<VkSurfaceFormatKHR>& surfaceFormats) {

        // 依次检查每个可用格式
        for (const auto& availableFormat : surfaceFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {  // 优先选择sRGB格式并配合非线性颜色空间，渲染结果更接近真实显示。
                return availableFormat;  // 找到合适格式后立即返回。
            }
        }

        return surfaceFormats[0];  // 如果没有最优格式，就退而求其次返回第一个可用格式。
    }

    // 从可用呈现模式中选择一种合适的显示同步策略。
    VkPresentModeKHR CzxSwapChain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {

        // mailbox，GPU满载，丢弃并覆盖较旧的帧缓冲，确保每次刷新呈现的图像都是输入指令后的结果，可以降低输入延迟，功耗高
        //for (const auto& availablePresentMode : availablePresentModes) {
        //    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
        //        std::cout << "Present mode: Mailbox" << std::endl;
        //        return availablePresentMode;
        //    }
        //}

        // immediate立即模式不与刷新同步，更新图像无同步，高功耗，会撕裂；了解性能和maxFPS用
        // for (const auto &availablePresentMode : availablePresentModes) {
        //   if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
        //     std::cout << "Present mode: Immediate" << std::endl;
        //     return availablePresentMode;
        //   }
        // }

        // FIFO，每次从帧缓冲队列获取队头图像，队列满时程序提交阻塞，GPU处理快时会导致闲置，直到下一次刷新交换链进行图像交换的framebuffer
        std::cout << "Present mode: V-Sync" << std::endl;  // 输出当前使用的同步策略，便于调试和确认。
        return VK_PRESENT_MODE_FIFO_KHR;  // 默认采用垂直同步模式，兼顾稳定性与兼容性。
    }

}   // namespace czx
