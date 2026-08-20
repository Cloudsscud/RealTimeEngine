#pragma once

#include "czx_device.h"
#include "czx_texture.h"

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
        static constexpr int MAX_FRAMES_IN_FLIGHT = 3;  // 同时允许两个帧在飞行中，常用于双缓冲/三重缓冲的同步

        CzxSwapChain(CzxDevice& deviceRef, VkExtent2D windowExtent);  // 使用当前窗口尺寸创建新的交换链。
        ~CzxSwapChain();  // 销毁交换链及其相关资源。

        CzxSwapChain(const CzxSwapChain&) = delete;  // 禁止拷贝，避免多个对象同时持有同一交换链资源。
        CzxSwapChain& operator=(const CzxSwapChain&) = delete;  // 禁止赋值，避免重复释放句柄。

        VkImage getImage(int index) { return m_images[index]; }
        VkImageView getImageView(int index) { return m_imageViews[index]; }
        VkImageView getDepthImageView(int index) { return m_depthTextures[index].m_imageView; }
        size_t imageCount() { return m_images.size(); }  // 返回交换链图像数量。
        VkFormat getSwapChainImageFormat() const{ return m_swapChainSurfaceFormat.format; }  // 返回交换链图像格式
        VkFormat getSwapChainDepthFormat()const { return m_device.physicalDevice().Selected().m_depthFormat; }  // 返回交换链图像格式
        VkExtent2D getSwapChainExtent() { return m_swapChainExtent; }  // 返回交换链尺寸。
        uint32_t width() { return m_swapChainExtent.width; }  // 返回交换链宽度。
        uint32_t height() { return m_swapChainExtent.height; }  // 返回交换链高度。

        float extentAspectRatio() {  // 计算当前交换链宽高比，方便透视投影等计算。
            return static_cast<float>(m_swapChainExtent.width) / static_cast<float>(m_swapChainExtent.height);
        }

        VkResult acquireNextImage(uint32_t* imageIndex);  // 从交换链中获取下一个可用于渲染的图像索引
        VkResult submitCommandBuffers(const VkCommandBuffer* buffers, uint32_t* imageIndex);  // 将命令缓冲区提交给当前图像渲染并配置呈现

        bool compareSwapChainFormats(const CzxSwapChain& swapChain)const {  // 比较两个交换链的颜色和深度格式，判断是否可直接复用。
            return swapChain.getSwapChainDepthFormat() == getSwapChainDepthFormat() &&
                swapChain.getSwapChainImageFormat() == getSwapChainImageFormat();
        }

    private:
        void init();
        void createSwapChain();  // 创建交换链对象、图像视图
        void createDepthResources();  // 创建深度缓冲资源
        void createSyncObjects();  // 创建信号量和栅栏，用于帧同步


        // 从表面属性选择交换链图像数量,默认双缓冲以避免画面撕裂
        static uint32_t chooseImageCount(const VkSurfaceCapabilitiesKHR& surfaceCaps);
        // 像素格式，从可用格式中选择合适的颜色格式和颜色空间
        static VkSurfaceFormatKHR chooseSwapSurfaceFormatAndColorSpace(const std::vector<VkSurfaceFormatKHR>& surfaceFormats);
        // 从可用呈现模式中选择最适合的显示模式
        static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);


        CzxDevice& m_device;
        std::vector<CzxTexture> m_depthTextures;    // 每图像对应的depth buffer


        VkExtent2D m_swapChainExtent;  // 交换链当前使用的屏幕范围
        VkSurfaceFormatKHR m_swapChainSurfaceFormat;
        VkPresentModeKHR m_swapChainPresentMode;

        VkSwapchainKHR m_swapChain;

        std::vector<VkImage> m_images;  // 交换链图像句柄集合，管理GPU内存申请的图像空间
        std::vector<VkImageView> m_imageViews;  // 交换链图像视图集合，管理相对应的图像的布局

        std::shared_ptr<CzxSwapChain> m_oldSwapChain;  // 旧交换链引用，用于重建时释放旧资源。

        // GPU同步
        std::vector<VkSemaphore> m_presentFinishedSemaphores;  // 该图像呈现完毕可以用于渲染
        std::vector<VkSemaphore> m_renderFinishedSemaphores;  // 表示该图像渲染完成可以用于呈现
        // CPU与GPU共享，在CPU上等待GPU完成
        std::vector<VkFence> m_inFlightFences;  // 表示该飞行帧是否阻塞
        std::vector<VkFence> m_imagesInFlight;  // 表示该图像当前属于哪一飞行帧
        size_t m_currentFrame = 0;  // 当前正在使用的飞行帧的索引
    };

}  // namespace czx
