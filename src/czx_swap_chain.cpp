#include "czx_swap_chain.h"

// std
#include <array>  // 提供固定大小数组容器，便于保存多个附件或同步对象的句柄。
#include <cstdlib>  // 提供通用C运行时函数，当前代码中未直接使用但可作为基础依赖。
#include <cstring>  // 提供内存操作函数，当前代码中未直接使用但和底层资源管理相关。
#include <iostream>  // 提供标准输出，便于打印当前选择的呈现模式等调试信息。
#include <limits>  // 提供数值极限常量，用于表示无效或未定义的窗口尺寸。
#include <set>  // 提供集合容器，便于管理和去重队列族相关信息。
#include <stdexcept>  // 提供运行时异常类型，便于在初始化失败时抛出明确错误。

namespace czx {

    // 构造函数会初始化交换链及其依赖资源，准备进入渲染流程。
    CzxSwapChain::CzxSwapChain(CzxDevice& deviceRef, VkExtent2D extent)  // 通过设备引用和当前窗口尺寸构造交换链对象。
        : m_device{ deviceRef }, m_windowExtent{ extent } {  // 把设备对象和窗口尺寸保存为成员变量，后续创建交换链时直接使用。
        init();  // 调用统一初始化流程，把交换链、视图、渲染通道和同步对象一次性创建好。
    }

    // 重建构造函数会基于旧交换链创建新的交换链，适用于窗口尺寸变化时的平滑替换。
    CzxSwapChain::CzxSwapChain(CzxDevice& deviceRef, VkExtent2D extent, std::shared_ptr<CzxSwapChain> previous)  // 通过旧交换链对象重建新交换链，适配窗口重建场景。
        : m_device{ deviceRef }, m_windowExtent{ extent }, m_oldSwapChain{previous} {  // 保存新尺寸和旧交换链句柄，后续创建时可继承旧资源状态。
        init();  // 先完成新交换链的创建流程。

        // 清理旧交换链直到不再使用
        m_oldSwapChain = nullptr;  // 旧交换链在新交换链建立后就不再需要，释放引用，避免额外持有。
    }

    // 初始化函数按顺序创建交换链、图像视图、渲染通道、深度资源和同步对象。
    void CzxSwapChain::init() {  // 这是统一的初始化入口，把交换链链路的所有依赖资源一次性准备好。
        createSwapChain();  // 初始化底层交换链对象，确定图像格式、尺寸和呈现模式。
        createImageViews();  // 为交换链中的每个图像创建可绑定到管线的ImageView。
        createRenderPass();  // 定义渲染过程中颜色附件和深度附件的布局与加载/保存规则。
        createDepthResources();  // 为深度测试准备对应的深度图像和视图。
        createFramebuffers();  // 把颜色附件和深度附件绑定到帧缓冲中，供渲染通道使用。
        createSyncObjects();  // 创建信号量和围栏，用于帧间同步和图像可用状态控制。
    }


    // 析构函数负责销毁交换链相关的图像视图、帧缓冲、渲染通道和同步对象。
    CzxSwapChain::~CzxSwapChain() {  // 对象销毁时释放交换链相关资源，避免句柄泄漏。
        for (auto imageView : m_swapChainImageViews) {  // 遍历所有交换链图像视图，逐个销毁。
            vkDestroyImageView(m_device.device(), imageView, nullptr);  // 释放每个图像视图占用的Vulkan对象。
        }
        m_swapChainImageViews.clear();  // 清空容器，避免悬空引用。

        if (m_swapChain != nullptr) {  // 只有当交换链对象确实存在时才销毁它。
            vkDestroySwapchainKHR(m_device.device(), m_swapChain, nullptr);  // 销毁底层交换链对象。
            m_swapChain = nullptr;  // 将句柄置空，避免重复释放。
        }

        for (int i = 0; i < m_depthImages.size(); i++) {  // 遍历所有深度图像资源，逐个释放图像、视图和内存。
            vkDestroyImageView(m_device.device(), m_depthImageViews[i], nullptr);  // 释放深度图像视图。
            vkDestroyImage(m_device.device(), m_depthImages[i], nullptr);  // 释放深度图像对象。
            vkFreeMemory(m_device.device(), m_depthImageMemorys[i], nullptr);  // 释放深度图像对应的设备内存。
        }

        for (auto framebuffer : m_swapChainFramebuffers) {  // 遍历所有帧缓冲，逐个销毁。
            vkDestroyFramebuffer(m_device.device(), framebuffer, nullptr);  // 释放帧缓冲对象。
        }

        vkDestroyRenderPass(m_device.device(), m_renderPass, nullptr);  // 销毁渲染通道对象，其他资源依赖它。

        // cleanup synchronization objects
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {  // 遍历每一帧的同步对象，依次释放。
            vkDestroySemaphore(m_device.device(), m_renderFinishedSemaphores[i], nullptr);  // 销毁渲染完成信号量。
            vkDestroySemaphore(m_device.device(), m_imageAvailableSemaphores[i], nullptr);  // 销毁图像可用信号量。
            vkDestroyFence(m_device.device(), m_inFlightFences[i], nullptr);  // 销毁帧围栏，用于同步提交队列。
        }
    }

    // 从交换链中获取一个可用于渲染的图像索引，并等待其可用。
    VkResult CzxSwapChain::acquireNextImage(uint32_t* imageIndex) {  // 这个函数用于拿到下一张可用于渲染的交换链图像。
        vkWaitForFences(
            m_device.device(),  // 传入逻辑设备句柄，等待当前帧围栏状态变为已提交。
            1,  // 只等待一个围栏对象。
            &m_inFlightFences[m_currentFrame],  // 等待当前帧对应的围栏，确保上一帧已完全结束。
            VK_TRUE,  // 使用阻塞等待，直到围栏被触发。
            std::numeric_limits<uint64_t>::max());  // 设置等待上限为最大值，表示一直等到可用。

        VkResult result = vkAcquireNextImageKHR(
            m_device.device(),  // 传入逻辑设备句柄。
            m_swapChain,  // 传入当前交换链对象。
            std::numeric_limits<uint64_t>::max(),  // 设置无限等待时间，直到有可用图像。
            m_imageAvailableSemaphores[m_currentFrame],  // 传入当前帧的图像可用信号量，等待图像准备好。
            VK_NULL_HANDLE,  // 不绑定额外的信号量或Fence。
            imageIndex);  // 输出获取到的图像索引。

        return result;  // 返回Vulkan获取图像的结果码，便于上层判断是否成功或需要重建交换链。
    }

    // 将命令缓冲区提交给当前图像并提交呈现请求，让渲染结果显示到窗口中。
    VkResult CzxSwapChain::submitCommandBuffers(
        const VkCommandBuffer* buffers, uint32_t* imageIndex) {  // 这个函数把本帧的命令缓冲区提交给GPU，并请求显示到交换链图像。
        if (m_imagesInFlight[*imageIndex] != VK_NULL_HANDLE) {  // 如果当前图像仍被上一帧占用，需要先等待它完成。
            vkWaitForFences(m_device.device(), 1, &m_imagesInFlight[*imageIndex], VK_TRUE, UINT64_MAX);  // 等待目标图像对应的上一帧围栏释放。
        }
        m_imagesInFlight[*imageIndex] = m_inFlightFences[m_currentFrame];  // 记录当前帧对该图像的占用关系。

        VkSubmitInfo submitInfo = {};  // 创建提交信息结构体，描述要提交哪些命令缓冲区和等待哪些信号量。
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;  // 标记结构体类型，方便Vulkan识别。

        VkSemaphore waitSemaphores[] = { m_imageAvailableSemaphores[m_currentFrame] };  // 设置等待的图像可用信号量。
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };  // 指定在颜色附件输出阶段之前等待信号量。
        submitInfo.waitSemaphoreCount = 1;  // 只有一个等待信号量。
        submitInfo.pWaitSemaphores = waitSemaphores;  // 指向等待信号量数组。
        submitInfo.pWaitDstStageMask = waitStages;  // 指定等待信号量触发时机。

        submitInfo.commandBufferCount = 1;  // 这次提交只包含一个命令缓冲区。
        submitInfo.pCommandBuffers = buffers;  // 指向当前要提交的命令缓冲区。

        VkSemaphore signalSemaphores[] = { m_renderFinishedSemaphores[m_currentFrame] };  // 设置渲染完成后要触发的信号量。
        submitInfo.signalSemaphoreCount = 1;  // 只触发一个信号量。
        submitInfo.pSignalSemaphores = signalSemaphores;  // 指向信号量数组。

        vkResetFences(m_device.device(), 1, &m_inFlightFences[m_currentFrame]);  // 重置当前帧围栏，准备下一次提交时重新使用。
        if (vkQueueSubmit(m_device.graphicsQueue(), 1, &submitInfo, m_inFlightFences[m_currentFrame]) !=
            VK_SUCCESS) {  // 把提交信息送到图形队列，使用当前帧围栏作为完成信号。
            throw std::runtime_error("failed to submit draw command buffer!");  // 提交失败时抛出异常。
        }

        VkPresentInfoKHR presentInfo = {};  // 创建呈现信息结构体，用于把渲染结果送给交换链显示。
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;  // 标记结构体类型。

        presentInfo.waitSemaphoreCount = 1;  // 等待一个信号量，确保渲染完成后再呈现。
        presentInfo.pWaitSemaphores = signalSemaphores;  // 指定要等待的渲染完成信号量。

        VkSwapchainKHR swapChains[] = { m_swapChain };  // 说明当前本次呈现使用的交换链对象。
        presentInfo.swapchainCount = 1;  // 只呈现一个交换链。
        presentInfo.pSwapchains = swapChains;  // 指向交换链数组。

        presentInfo.pImageIndices = imageIndex;  // 指定当前要显示哪一张交换链图像。

        auto result = vkQueuePresentKHR(m_device.presentQueue(), &presentInfo);  // 把当前图像提交到呈现队列。

        m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;  // 切换到下一帧，循环复用同步对象。

        return result;  // 返回呈现结果，供上层判断是否需要重建交换链。
    }

    // 创建底层交换链对象，并根据窗口和物理设备能力选择合适的格式、模式和尺寸。
    void CzxSwapChain::createSwapChain() {  // 这个函数从设备能力和窗口信息中选出最合适的交换链参数并创建交换链。
        SwapChainSupportDetails swapChainSupport = m_device.getSwapChainSupport();  // 获取当前物理设备对交换链的支持细节。

        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);  // 选择颜色格式。
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);  // 选择呈现模式。
        VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);  // 选择交换链尺寸。

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;  // 至少多申请一张图像，避免前后帧竞争。
        if (swapChainSupport.capabilities.maxImageCount > 0 &&
            imageCount > swapChainSupport.capabilities.maxImageCount) {  // 如果超过最大值，就截断到最大值。
            imageCount = swapChainSupport.capabilities.maxImageCount;  // 限制实际创建的图像数量。
        }

        VkSwapchainCreateInfoKHR createInfo = {};  // 创建交换链创建信息结构体，保存所有创建参数。
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;  // 标记结构体类型。
        createInfo.surface = m_device.surface();  // 指定当前窗口对应的Vulkan表面。

        createInfo.minImageCount = imageCount;  // 设置交换链至少包含多少张图像。
        createInfo.imageFormat = surfaceFormat.format;  // 设置交换链图像格式。
        createInfo.imageColorSpace = surfaceFormat.colorSpace;  // 设置颜色空间。
        createInfo.imageExtent = extent;  // 设置交换链的实际宽高。
        createInfo.imageArrayLayers = 1;  // 这里使用单层图像，适合普通显示目标。
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;  // 说明这些图像会被当作颜色附件用于渲染。

        QueueFamilyIndices indices = m_device.findPhysicalQueueFamilies();  // 获取图形和呈现队列族索引。
        uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value()};  // 把两个队列族索引组合成数组。

        if (indices.graphicsFamily != indices.presentFamily) {  // 当图形和呈现不在同一个队列族时，使用并发共享模式。
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;  // 允许图像在两个队列族之间共享。
            createInfo.queueFamilyIndexCount = 2;  // 指定共享队列族数量为2。
            createInfo.pQueueFamilyIndices = queueFamilyIndices;  // 指向共享队列族索引数组。
        }
        else {  // 若两个队列族相同，则使用独占模式，减少同步开销。
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;  // 指定图像只由一个队列族拥有。
            createInfo.queueFamilyIndexCount = 0;      // Optional  // 不指定额外队列族，保持默认。
            createInfo.pQueueFamilyIndices = nullptr;  // Optional  // 不使用额外队列族数组。
        }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;  // 保留表面当前的预变换方式。
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;  // 设置合成方式为不透明，适合普通窗口显示。

        createInfo.presentMode = presentMode;  // 设置实际使用的呈现模式。
        createInfo.clipped = VK_TRUE;  // 开启裁剪，避免渲染内容遮挡窗口外的区域。

        createInfo.oldSwapchain = m_oldSwapChain == nullptr ? VK_NULL_HANDLE : m_oldSwapChain->m_swapChain;  // 如果存在旧交换链，就将其作为重建时的旧对象。

        if (vkCreateSwapchainKHR(m_device.device(), &createInfo, nullptr, &m_swapChain) != VK_SUCCESS) {  // 根据配置真正创建底层交换链对象。
            throw std::runtime_error("failed to create swap chain!");  // 创建失败时抛出异常。
        }

        // we only specified a minimum number of images in the swap chain, so the implementation is
        // allowed to create a swap chain with more. That's why we'll first query the final number of
        // images with vkGetSwapchainImagesKHR, then resize the container and finally call it again to
        // retrieve the handles.
        vkGetSwapchainImagesKHR(m_device.device(), m_swapChain, &imageCount, nullptr);  // 先查询实际创建出来的图像数量。
        m_swapChainImages.resize(imageCount);  // 根据实际数量调整容器大小。
        vkGetSwapchainImagesKHR(m_device.device(), m_swapChain, &imageCount, m_swapChainImages.data());  // 把交换链中所有图像句柄读出来保存。

        m_swapChainImageFormat = surfaceFormat.format;  // 保存当前交换链图像格式，后续创建ImageView和RenderPass时要复用。
        m_swapChainExtent = extent;  // 保存当前交换链实际大小，后续渲染通道和帧缓冲都依赖它。
    }

    // 为交换链中的每个图像创建对应的VkImageView，供渲染通道和帧缓冲使用。
    void CzxSwapChain::createImageViews() {  // 这个函数把交换链中的每个图像包装成可绑定到管线的ImageView。
        m_swapChainImageViews.resize(m_swapChainImages.size());  // 先按图像数量准备足够数量的ImageView句柄容器。
        for (size_t i = 0; i < m_swapChainImages.size(); i++) {  // 遍历每张交换链图像，逐个创建ImageView。
            VkImageViewCreateInfo viewInfo{};  // 创建视图创建信息结构体，描述ImageView的细节。
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;  // 标记结构体类型。
            viewInfo.image = m_swapChainImages[i];  // 指定要创建视图的源图像句柄。
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;  // 说明这是二维图像视图，适合普通屏幕目标。
            viewInfo.format = m_swapChainImageFormat;  // 复用交换链图像格式，保证视图与图像一致。
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;  // 表示当前视图关注的是颜色通道。
            viewInfo.subresourceRange.baseMipLevel = 0;  // 从第0级mipmap开始。
            viewInfo.subresourceRange.levelCount = 1;  // 只使用1级mipmap。
            viewInfo.subresourceRange.baseArrayLayer = 0;  // 从第0层数组开始。
            viewInfo.subresourceRange.layerCount = 1;  // 只使用1层。

            if (vkCreateImageView(m_device.device(), &viewInfo, nullptr, &m_swapChainImageViews[i]) !=
                VK_SUCCESS) {  // 根据配置真正创建图像视图对象。
                throw std::runtime_error("failed to create texture image view!");  // 创建失败时抛出异常。
            }
        }
    }

    // 创建渲染通道，描述颜色附件和深度附件的加载、存储和布局转换规则。
    void CzxSwapChain::createRenderPass() {  // 这个函数定义整个渲染过程中的附件布局和依赖关系。
        VkAttachmentDescription depthAttachment{};  // 创建深度附件描述结构体，定义深度缓冲的使用方式。
        depthAttachment.format = findDepthFormat();  // 选择当前设备支持的深度格式。
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;  // 关闭多重采样，使用单采样点。
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;  // 每次渲染前先清空深度附件内容。
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;  // 深度附件不需要存储最终结果，后续不再使用。
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;  // 模板缓冲不需要加载操作。
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;  // 模板缓冲不需要存储操作。
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // 初始布局未定义，渲染前会重新进行布局转换。
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;  // 结束后布局适合深度附件使用。

        VkAttachmentReference depthAttachmentRef{};  // 创建深度附件引用，说明子通道如何使用该附件。
        depthAttachmentRef.attachment = 1;  // 该引用对应第二个附件，也就是深度附件。
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;  // 指定子通道访问该附件时使用的布局。

        VkAttachmentDescription colorAttachment = {};  // 创建颜色附件描述结构体，定义交换链图像作为颜色输出时的规则。
        colorAttachment.format = getSwapChainImageFormat();  // 复用交换链图像格式，保证与颜色目标一致。
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;  // 关闭多重采样。
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;  // 每次开始渲染时清空颜色缓冲。
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;  // 渲染后需要保存颜色结果，供显示。
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;  // 模板缓冲不需要存储。
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;  // 模板缓冲不需要加载。
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // 初始布局未定义，等渲染前转换.
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  // 渲染结束后布局适合提交到交换链显示。

        VkAttachmentReference colorAttachmentRef = {};  // 创建颜色附件引用，说明子通道如何访问颜色附件。
        colorAttachmentRef.attachment = 0;  // 该引用对应第一个附件，也就是颜色附件。
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;  // 指定子通道访问颜色附件时使用的布局。

        VkSubpassDescription subpass = {};  // 创建子通道描述，定义当前渲染过程使用哪种绑定点和哪些附件。
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;  // 指定这是图形渲染子通道。
        subpass.colorAttachmentCount = 1;  // 该子通道只使用一个颜色附件。
        subpass.pColorAttachments = &colorAttachmentRef;  // 指向颜色附件引用。
        subpass.pDepthStencilAttachment = &depthAttachmentRef;  // 指向深度附件引用，启用深度测试。

        VkSubpassDependency dependency = {};  // 创建子通道依赖，定义前后子通道之间的访问和阶段同步要求。
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;  // 依赖来自外部，也就是上一个渲染流程或外部操作。
        dependency.srcAccessMask = 0;  // 外部阶段刚开始时没有访问权限要求。
        dependency.srcStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;  // 说明外部阶段需要在颜色输出和早期深度测试阶段完成。
        dependency.dstSubpass = 0;  // 目标子通道是当前主子通道。
        dependency.dstStageMask =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;  // 目标子通道在颜色输出和早期深度测试阶段执行。
        dependency.dstAccessMask =
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;  // 允许目标子通道进行颜色和深度写入。

        std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };  // 把颜色和深度附件打包成数组，传给渲染通道创建信息。
        VkRenderPassCreateInfo renderPassInfo = {};  // 创建渲染通道创建信息结构体。
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;  // 标记结构体类型。
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());  // 设置附件数量。
        renderPassInfo.pAttachments = attachments.data();  // 指向附件数组。
        renderPassInfo.subpassCount = 1;  // 只定义一个子通道。
        renderPassInfo.pSubpasses = &subpass;  // 指向子通道描述。
        renderPassInfo.dependencyCount = 1;  // 只有一个依赖关系。
        renderPassInfo.pDependencies = &dependency;  // 指向依赖描述。

        if (vkCreateRenderPass(m_device.device(), &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {  // 根据上面的配置真正创建渲染通道对象。
            throw std::runtime_error("failed to create render pass!");  // 创建失败时抛出异常。
        }
    }

    // 为每个交换链图像创建对应的帧缓冲对象，绑定颜色和深度附件。
    void CzxSwapChain::createFramebuffers() {  // 这个函数为每个交换链图像创建一个可用于渲染通道的帧缓冲。
        m_swapChainFramebuffers.resize(imageCount());  // 先按交换链图像数量准备帧缓冲容器。
        for (size_t i = 0; i < imageCount(); i++) {  // 遍历每张交换链图像，创建对应帧缓冲。
            std::array<VkImageView, 2> attachments = { m_swapChainImageViews[i], m_depthImageViews[i] };  // 把颜色视图和深度视图组合成一组附件。

            VkExtent2D swapChainExtent = getSwapChainExtent();  // 获取当前交换链尺寸，帧缓冲尺寸需与之一致。
            VkFramebufferCreateInfo framebufferInfo = {};  // 创建帧缓冲创建信息结构体。
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;  // 标记结构体类型。
            framebufferInfo.renderPass = m_renderPass;  // 指定当前帧缓冲绑定到哪个渲染通道。
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());  // 设置附件数量。
            framebufferInfo.pAttachments = attachments.data();  // 指向附件数组。
            framebufferInfo.width = swapChainExtent.width;  // 设置帧缓冲宽度。
            framebufferInfo.height = swapChainExtent.height;  // 设置帧缓冲高度。
            framebufferInfo.layers = 1;  // 只使用单层，适合普通2D渲染目标。

            if (vkCreateFramebuffer(
                m_device.device(),
                &framebufferInfo,
                nullptr,
                &m_swapChainFramebuffers[i]) != VK_SUCCESS) {  // 根据配置创建帧缓冲对象。
                throw std::runtime_error("failed to create framebuffer!");  // 创建失败时抛出异常。
            }
        }
    }

    // 创建深度缓冲资源，供渲染通道中的深度测试使用。
    void CzxSwapChain::createDepthResources() {  // 这个函数为深度测试准备图像资源和对应视图。
        VkFormat depthFormat = findDepthFormat();  // 先选择一个当前设备支持的深度格式。
        m_swapChainDepthFormat = depthFormat;  // 保存深度格式，后续其他代码可直接使用。
        VkExtent2D swapChainExtent = getSwapChainExtent();  // 获取当前交换链尺寸，深度图像需要与之一致。

        m_depthImages.resize(imageCount());  // 按交换链图像数量准备深度图像数组。
        m_depthImageMemorys.resize(imageCount());  // 按数量准备深度图像内存句柄数组。
        m_depthImageViews.resize(imageCount());  // 按数量准备深度图像视图数组。

        for (int i = 0; i < m_depthImages.size(); i++) {  // 遍历每一帧对应的深度资源，逐个创建。
            VkImageCreateInfo imageInfo{};  // 创建图像创建信息结构体，描述深度图像的尺寸、格式和用途。
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;  // 标记结构体类型。
            imageInfo.imageType = VK_IMAGE_TYPE_2D;  // 使用二维图像，适合深度缓冲。
            imageInfo.extent.width = swapChainExtent.width;  // 设置图像宽度。
            imageInfo.extent.height = swapChainExtent.height;  // 设置图像高度。
            imageInfo.extent.depth = 1;  // 深度维度为1，表示二维平面。
            imageInfo.mipLevels = 1;  // 只使用一级mipmap。
            imageInfo.arrayLayers = 1;  // 只使用一层。
            imageInfo.format = depthFormat;  // 使用上面找到的深度格式。
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;  // 使用最优平铺，适合设备内存和附件使用。
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // 初始布局未定义，渲染前会转换。
            imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;  // 说明该图像将作为深度模板附件使用。
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;  // 关闭多重采样。
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;  // 深度图像只属于一个队列族。
            imageInfo.flags = 0;  // 不使用额外的图像创建标志。

            m_device.createImageWithInfo(
                imageInfo,  // 传入图像创建信息。
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,  // 指定使用设备本地内存，适合高频渲染附件。
                m_depthImages[i],  // 输出参数，接收创建出的图像句柄。
                m_depthImageMemorys[i]);  // 输出参数，接收图像内存句柄。

            VkImageViewCreateInfo viewInfo{};  // 创建深度图像的视图创建信息结构体。
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;  // 标记结构体类型。
            viewInfo.image = m_depthImages[i];  // 指定要创建视图的深度图像句柄。
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;  // 创建二维图像视图。
            viewInfo.format = depthFormat;  // 使用深度格式。
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;  // 表示当前视图关注的是深度通道。
            viewInfo.subresourceRange.baseMipLevel = 0;  // 从第0级mipmap开始。
            viewInfo.subresourceRange.levelCount = 1;  // 只使用1级。
            viewInfo.subresourceRange.baseArrayLayer = 0;  // 从第0层数组开始。
            viewInfo.subresourceRange.layerCount = 1;  // 只使用1层。

            if (vkCreateImageView(m_device.device(), &viewInfo, nullptr, &m_depthImageViews[i]) != VK_SUCCESS) {  // 根据配置创建深度图像视图。
                throw std::runtime_error("failed to create texture image view!");  // 创建失败时抛出异常。
            }
        }
    }

    // 创建信号量和围栏，用于控制图像可用、渲染完成以及帧间同步。
    void CzxSwapChain::createSyncObjects() {  // 这个函数准备每帧渲染时使用的同步对象。
        m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);  // 按最大帧数准备图像可用信号量数组。
        m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);  // 按最大帧数准备渲染完成信号量数组。
        m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);  // 按最大帧数准备围栏数组。
        m_imagesInFlight.resize(imageCount(), VK_NULL_HANDLE);  // 按交换链图像数量准备每张图像的占用记录。

        VkSemaphoreCreateInfo semaphoreInfo = {};  // 创建信号量创建信息结构体。
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;  // 标记结构体类型。

        VkFenceCreateInfo fenceInfo = {};  // 创建围栏创建信息结构体。
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;  // 标记结构体类型。
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // 初始化时让围栏处于已触发状态，避免第一次提交时卡住。

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {  // 遍历每一帧，创建对应的同步对象。
            if (vkCreateSemaphore(m_device.device(), &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) !=
                VK_SUCCESS ||
                vkCreateSemaphore(m_device.device(), &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) !=
                VK_SUCCESS ||
                vkCreateFence(m_device.device(), &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {  // 同时创建图像可用信号量、渲染完成信号量和围栏。
                throw std::runtime_error("failed to create synchronization objects for a frame!");  // 任一对象创建失败时抛出异常。
            }
        }
    }

    // 从可用格式中选择一个最适合当前窗口和显示器的颜色格式。
    VkSurfaceFormatKHR CzxSwapChain::chooseSwapSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR>& availableFormats) {  // 这个函数从系统支持的颜色格式中挑选最适合当前项目的格式。
        for (const auto& availableFormat : availableFormats) {  // 依次检查每个可用格式。
            // gamma矫正
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {  // 优先选择sRGB格式并配合非线性颜色空间，渲染结果更接近真实显示。
                return availableFormat;  // 找到合适格式后立即返回。
            }
        }

        return availableFormats[0];  // 如果没有最优格式，就退而求其次返回第一个可用格式。
    }

    // 从可用呈现模式中选择一种合适的显示同步策略。
    VkPresentModeKHR CzxSwapChain::chooseSwapPresentMode(
        const std::vector<VkPresentModeKHR>& availablePresentModes) {  // 这个函数用于选择交换链的显示同步方式。

        // FIFO，GPU处理快时会闲置，直到下一周期交换链进行图像交换的buffer

        //mailbox，GPU满载，丢弃并覆盖较旧的后缓冲，可以降低输入延迟，功耗高
        //for (const auto& availablePresentMode : availablePresentModes) {
        //    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
        //        std::cout << "Present mode: Mailbox" << std::endl;
        //        return availablePresentMode;
        //    }
        //}

        // immediate立即模式不与刷新同步，更新图像无同步，高功耗，撕裂；了解性能和maxFPS用
        // for (const auto &availablePresentMode : availablePresentModes) {
        //   if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
        //     std::cout << "Present mode: Immediate" << std::endl;
        //     return availablePresentMode;
        //   }
        // }

        std::cout << "Present mode: V-Sync" << std::endl;  // 输出当前使用的同步策略，便于调试和确认。
        return VK_PRESENT_MODE_FIFO_KHR;  // 默认采用垂直同步模式，兼顾稳定性与兼容性。
    }

    // 根据窗口尺寸和表面能力计算交换链实际使用的宽高。
    VkExtent2D CzxSwapChain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {  // 这个函数根据表面能力和当前窗口大小计算交换链的实际尺寸。
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {  // 如果表面已经给出有效当前尺寸，就直接使用它。
            return capabilities.currentExtent;  // 返回表面当前的可用尺寸。
        }
        else {  // 如果表面没有提供固定大小，就根据窗口尺寸和范围限制计算实际尺寸。
            VkExtent2D actualExtent = m_windowExtent;  // 先使用当前窗口大小作为初始尺寸。
            actualExtent.width = std::max(
                capabilities.minImageExtent.width,  // 限制最小宽度，避免尺寸过小导致问题。
                std::min(capabilities.maxImageExtent.width, actualExtent.width));  // 限制最大宽度，避免尺寸过大超出表面能力。
            actualExtent.height = std::max(
                capabilities.minImageExtent.height,  // 限制最小高度。
                std::min(capabilities.maxImageExtent.height, actualExtent.height));  // 限制最大高度。

            return actualExtent;  // 返回裁剪后的实际交换链尺寸。
        }
    }

    // 查找当前设备支持的深度格式，供深度附件创建时使用。
    VkFormat CzxSwapChain::findDepthFormat() {  // 这个函数从一组候选格式中选出设备支持的深度格式。
        return m_device.findSupportedFormat(
            { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },  // 依次尝试几种常见的深度格式。
            VK_IMAGE_TILING_OPTIMAL,  // 使用最优平铺，适合深度附件。
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);  // 只选择支持深度模板附件用途的格式。
    }

}   // namespace czx
