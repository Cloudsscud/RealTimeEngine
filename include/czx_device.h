#pragma once

#include "czx_window.h"

// std
#include <string>
#include <vector>
#include <optional>

namespace czx {

    // 记录当前物理设备对交换链所支持的能力集合，供创建交换链时选择合适配置。
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;  // 设备对当前窗口表面的基础能力限制，如最小/最大图像数和尺寸范围。
        std::vector<VkSurfaceFormatKHR> formats;  // 可用的颜色格式和颜色空间集合。
        std::vector<VkPresentModeKHR> presentModes;  // 可用的呈现模式集合，如FIFO、Mailbox等。
    };

    // 记录图形队列和呈现队列在物理设备中的队列族索引，方便后续创建逻辑设备和提交命令。
    struct QueueFamilyIndices {
        // 通过optional检查是否找到相应的队列族索引
        std::optional<uint32_t> graphicsFamily;  // 图形/计算队列族索引。
        std::optional<uint32_t> presentFamily;  // 负责呈现到窗口表面的队列族索引。
        bool isComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }  // 判断是否同时具备图形和呈现能力。
    };

    // 负责初始化Vulkan实例、创建窗口表面、选择物理设备、创建逻辑设备以及命令池等核心资源。
    class CzxDevice {
    public:
#ifdef NDEBUG
        const bool enableValidationLayers = false;
#else
        const bool enableValidationLayers = true;
#endif

        CzxDevice(CzxWindow& window);
        ~CzxDevice();

        // 禁拷贝与移动
        CzxDevice(const CzxDevice&) = delete;
        CzxDevice& operator=(const CzxDevice&) = delete;
        CzxDevice(CzxDevice&&) = delete;
        CzxDevice& operator=(CzxDevice&&) = delete;

        VkCommandPool getCommandPool() { return m_commandPool; }  // 返回命令池句柄，供渲染器和资源管理器分配命令缓冲区。
        VkDevice device() { return m_device; }  // 返回逻辑设备句柄，便于调用Vulkan API创建和销毁资源。
        VkSurfaceKHR surface() { return m_surface; }  // 返回窗口表面句柄，供交换链和呈现流程使用。
        VkQueue graphicsQueue() { return m_graphicsQueue; }  // 返回图形队列句柄，用于提交绘制相关命令。
        VkQueue presentQueue() { return m_presentQueue; }  // 返回呈现队列句柄，用于将图像提交到窗口表面。

        SwapChainSupportDetails getSwapChainSupport() { return querySwapChainSupport(m_physicalDevice); }  // 通过当前物理设备查询交换链相关能力。
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);  // 根据内存属性筛选出合适的内存类型。
        QueueFamilyIndices findPhysicalQueueFamilies() { return findQueueFamilies(m_physicalDevice); }  // 查找当前物理设备所支持的队列族。
        VkFormat findSupportedFormat(
            const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);  // 在候选格式中挑选当前设备支持的格式。
        VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
        const VkPhysicalDeviceProperties& getPhysicalDeviceProperties() const { return m_properties; }

        // Buffer Helper Functions
        void createBuffer(  // 创建一个Vulkan缓冲区并为其分配内存，供顶点、索引或上传数据使用。
            VkDeviceSize size,
            VkBufferUsageFlags usage,
            VkMemoryPropertyFlags properties,
            VkBuffer& buffer,
            VkDeviceMemory& bufferMemory);
        VkCommandBuffer beginSingleTimeCommands();  // 开始一次性提交的命令缓冲区，用于执行一次性GPU操作。
        void endSingleTimeCommands(VkCommandBuffer commandBuffer);  // 结束一次性命令缓冲区并提交到队列执行。
        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);  // 将一个缓冲区中的内容复制到另一个缓冲区。
        void copyBufferToImage(  // 将缓冲区内容拷贝到图像中，常用于上传纹理或贴图数据。
            VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount);

        void createImageWithInfo(  // 创建图像并分配其内存，适用于深度缓冲和纹理等资源。
            const VkImageCreateInfo& imageInfo,
            VkMemoryPropertyFlags properties,
            VkImage& image,
            VkDeviceMemory& imageMemory);

        // 创建图像视图
        VkImageView createImageView(
            VkImage image,
            VkFormat format,
            VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            uint32_t mipLevels = 1,
            uint32_t baseMipLevel = 0,
            uint32_t arrayLayers = 1,
            uint32_t baseArrayLayer = 0,
            VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D);

        // 创建采样器（使用默认参数，可选传递自定义创建信息）
        VkSampler createSampler(const VkSamplerCreateInfo& samplerInfo);
        // 或者提供一个简化版本，使用常用配置
        VkSampler createDefaultSampler();

        // 通用图像布局转换
        void transitionImageLayout(
            VkImage image,
            VkFormat format,
            VkImageLayout oldLayout,
            VkImageLayout newLayout,
            uint32_t mipLevels = 1,
            uint32_t baseMipLevel = 0,
            uint32_t arrayLayers = 1,
            uint32_t baseArrayLayer = 0,
            VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);

        VkPhysicalDeviceProperties m_properties;  // 保存当前物理设备的属性，如名称、类型和限制。

    private:
        void createInstance();
        void setupDebugMessenger();
        void createSurface();
        void pickPhysicalDevice();
        void createLogicalDevice();
        void createCommandPool();

        // helper functions
        bool isDeviceSuitable(VkPhysicalDevice device);
        std::vector<const char*> getRequiredExtensions();   // 获取系统需要的所有拓展名
        bool checkValidationLayerSupport(); // 检查系统的验证层是否可用
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
        void hasGflwRequiredInstanceExtensions();   // 验证需求的拓展是否完善
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device); // 在物理设备中查找创建交换链需要的显示器的能力

        VkInstance m_instance;  // Vulkan实例句柄，整个应用的入口对象。
        VkDebugUtilsMessengerEXT m_debugMessenger;  // 调试消息回调句柄，用于输出验证层错误和警告。
        VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;  // 当前选择的物理设备（显卡）句柄。
        CzxWindow& m_window;  // 对窗口对象的引用，用于创建窗口表面和查询窗口尺寸。
        VkCommandPool m_commandPool;  // 命令池，负责分配和管理命令缓冲区。

        VkDevice m_device;  // 逻辑设备句柄，后续所有资源创建都依赖它。
        VkSurfaceKHR m_surface;  // 窗口表面句柄，用于交换链和呈现。
        VkQueue m_graphicsQueue;  // 图形队列句柄。
        VkQueue m_presentQueue;  // 呈现队列句柄。

        const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };  // 调试时启用的验证层名称。
        const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };  // 创建交换链需要启用的扩展。
    };

}  // namespace czx