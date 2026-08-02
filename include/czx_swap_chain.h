#pragma once

#include "czx_device.h"

// vulkan headers
#include <vulkan/vulkan.h>

// std lib headers
#include <memory>
#include <string>
#include <vector>

namespace czx {

    // 负责管理交换链、图像视图、深度缓冲、渲染通道和同步对象，是渲染流程中最核心的呈现基础设施之一。
    class CzxSwapChain {
    public:
        static constexpr int MAX_FRAMES_IN_FLIGHT = 2;  // 同时允许两个帧在飞行中，常用于双缓冲/三重缓冲的同步。

        CzxSwapChain(CzxDevice& deviceRef, VkExtent2D windowExtent);  // 使用当前窗口尺寸创建新的交换链。
        CzxSwapChain(CzxDevice& deviceRef, VkExtent2D windowExtent, std::shared_ptr<CzxSwapChain> previous);  // 通过旧交换链重建新交换链，便于窗口尺寸变化时平滑替换。
        ~CzxSwapChain();  // 销毁交换链及其相关资源。

        CzxSwapChain(const CzxSwapChain&) = delete;  // 禁止拷贝，避免多个对象同时持有同一交换链资源。
        CzxSwapChain& operator=(const CzxSwapChain&) = delete;  // 禁止赋值，避免重复释放句柄。

        VkFramebuffer getFrameBuffer(int index) { return m_swapChainFramebuffers[index]; }  // 返回指定图像索引对应的帧缓冲对象。
        VkRenderPass getRenderPass() { return m_renderPass; }  // 返回渲染通道句柄，供渲染器创建渲染过程。
        VkImageView getImageView(int index) { return m_swapChainImageViews[index]; }  // 返回指定交换链图像的视图句柄。
        size_t imageCount() { return m_swapChainImages.size(); }  // 返回交换链图像数量。
        VkFormat getSwapChainImageFormat() { return m_swapChainImageFormat; }  // 返回交换链图像格式。
        VkExtent2D getSwapChainExtent() { return m_swapChainExtent; }  // 返回交换链尺寸。
        uint32_t width() { return m_swapChainExtent.width; }  // 返回交换链宽度。
        uint32_t height() { return m_swapChainExtent.height; }  // 返回交换链高度。

        float extentAspectRatio() {  // 计算当前交换链宽高比，方便透视投影等计算。
            return static_cast<float>(m_swapChainExtent.width) / static_cast<float>(m_swapChainExtent.height);
        }
        VkFormat findDepthFormat();  // 查找当前设备支持的深度缓冲格式。

        VkResult acquireNextImage(uint32_t* imageIndex);  // 从交换链中获取下一个可用于渲染的图像索引。
        VkResult submitCommandBuffers(const VkCommandBuffer* buffers, uint32_t* imageIndex);  // 将命令缓冲区提交给当前图像并呈现。

        bool compareSwapChainFormats(const CzxSwapChain& swapChain)const {  // 比较两个交换链的颜色和深度格式，判断是否可直接复用。
            return swapChain.m_swapChainDepthFormat == m_swapChainDepthFormat &&
                swapChain.m_swapChainImageFormat == m_swapChainImageFormat;
        }

    private:
        void init();  // 按顺序完成交换链的初始化流程。
        void createSwapChain();  // 创建底层交换链对象。
        void createImageViews();  // 为交换链图像创建对应的VkImageView。
        void createDepthResources();  // 创建深度缓冲资源，供深度测试使用。
        void createRenderPass();  // 创建渲染通道，描述颜色和深度附件的操作。
        void createFramebuffers();  // 为每个交换链图像创建对应帧缓冲对象。
        void createSyncObjects();  // 创建信号量和栅栏，用于帧同步。

        // Helper functions
        VkSurfaceFormatKHR chooseSwapSurfaceFormat(  // 从可用格式中选择合适的颜色格式和颜色空间。
            const std::vector<VkSurfaceFormatKHR>& availableFormats);
        VkPresentModeKHR chooseSwapPresentMode(  // 从可用呈现模式中选择最适合的显示模式。
            const std::vector<VkPresentModeKHR>& availablePresentModes);
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);  // 根据窗口大小和设备限制选择交换链尺寸。

        VkFormat m_swapChainImageFormat;  // 交换链图像的颜色格式。
        VkFormat m_swapChainDepthFormat;  // 深度缓冲图像格式。
        VkExtent2D m_swapChainExtent;  // 交换链当前使用的尺寸。

        std::vector<VkFramebuffer> m_swapChainFramebuffers;  // 每个交换链图像对应的帧缓冲对象。
        VkRenderPass m_renderPass;  // 当前渲染通道句柄。

        std::vector<VkImage> m_depthImages;  // 深度图像句柄集合。
        std::vector<VkDeviceMemory> m_depthImageMemorys;  // 深度图像内存对象集合。
        std::vector<VkImageView> m_depthImageViews;  // 深度图像视图集合。
        std::vector<VkImage> m_swapChainImages;  // 交换链图像句柄集合。
        std::vector<VkImageView> m_swapChainImageViews;  // 交换链图像视图集合。

        CzxDevice& m_device;  // 对设备对象的引用，所有资源都依托于这个逻辑设备。
        VkExtent2D m_windowExtent;  // 当前窗口的尺寸。

        VkSwapchainKHR m_swapChain;  // Vulkan交换链句柄。
        std::shared_ptr<CzxSwapChain> m_oldSwapChain;  // 旧交换链引用，用于重建时释放旧资源。

        std::vector<VkSemaphore> m_imageAvailableSemaphores;  // 用于表示图像可用的信号量。
        std::vector<VkSemaphore> m_renderFinishedSemaphores;  // 用于表示渲染完成的信号量。
        std::vector<VkFence> m_inFlightFences;  // 每帧对应的提交完成栅栏。
        std::vector<VkFence> m_imagesInFlight;  // 跟踪每个图像当前属于哪一帧。
        size_t m_currentFrame = 0;  // 当前正在使用的帧索引。
    };

}  // namespace czx
