#pragma once

#include "czx_window.h"

#include <vulkan/vulkan.h>

// std
#include <string>
#include <vector>
#include <optional>

namespace czx {

    // 物理设备及其属性
    struct PhysicalDevice {
        VkPhysicalDevice m_physDevice;  // 该物理设备句柄
        VkPhysicalDeviceProperties m_devProps;  // 属性
        std::vector<VkQueueFamilyProperties> m_qFamilyProps; // 队列家族属性
        std::vector<VkBool32> m_qSupportsPresent;    // 记录对应的队列家族是否支持呈现
        std::vector<VkSurfaceFormatKHR> m_surfaceFormats;   // 支持的表面格式:格式与色彩空间:像素的比特编码格式和色彩空间
        VkSurfaceCapabilitiesKHR m_surfaceCaps; // 表面能力（图像数量、尺寸最值限制、支持的一些能力）
        VkPhysicalDeviceMemoryProperties m_memProps; // 内存属性
        std::vector<VkPresentModeKHR> m_presentModes;   // 可用的呈现模式集合，如FIFO、Mailbox
        VkPhysicalDeviceFeatures m_features;    // 支持的功能
        VkFormat m_depthFormat; // 物理设备偏好的深度格式
    };

    // 记录图形队列和呈现队列在物理设备中的队列族索引，方便后续创建逻辑设备和提交命令。
    struct QueueFamilyIndices {
        // 通过optional检查是否找到相应的队列族索引
        std::optional<uint32_t> graphicsFamily;  // 图形/计算队列族索引。
        std::optional<uint32_t> presentFamily;  // 负责呈现到窗口表面的队列族索引。
        bool isComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }  // 判断是否同时具备图形和呈现能力。
    };

    class CzxPhysicalDevices {
    public:
        CzxPhysicalDevices() {}
        ~CzxPhysicalDevices() {}
        void Init(const VkInstance& instance, const VkSurfaceKHR& surface);

        // 检查是否有设备满足需求:有需求的队列、支持显示功能
        QueueFamilyIndices SelectDevice(VkQueueFlags RequiredQueueType, bool SupportsPresent);
        // 获取的那个满足需求的设备
        const PhysicalDevice& Selected() const;
        
    private:
        void PrintImageUsageFlags(const VkImageUsageFlags& flags);
        void PrintMemoryProperty(const VkMemoryPropertyFlags& flags);

        std::vector<PhysicalDevice> m_devices;  // 支持的所有物理设备
        int m_devIndex = -1;   // 选中用于渲染的设备
    };

    // 记录当前物理设备对交换链所支持的能力集合，供创建交换链时选择合适配置。
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;  // 设备对当前窗口表面的基础能力限制，如最小/最大图像数和尺寸范围。
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
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

        VkCommandPool getCommandPool() { return m_commandPool; }  // 命令池
        VkInstance getInstance() const { return m_instance; }  // 命令池

        const CzxPhysicalDevices& physicalDevice() const { return m_physicalDevices; }  // 物理设备
        const VkDevice& device() const { return m_device; }  // 逻辑设备
        const VkSurfaceKHR& surface() const { return m_surface; }  // 窗口表面
        uint32_t graphicsQueueFamily() const { return m_qFamilyIndices.graphicsFamily.value(); }  // 图形队列族索引
        uint32_t presentQueueFamily() const { return m_qFamilyIndices.presentFamily.value(); }  // 呈现队列族索引


        // 创建图像视图
        VkImageView createImageView(
            VkImage image, VkFormat format,
            VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D,
            uint32_t layerCount = 1, uint32_t mipLevels = 1);

        void createCommandBuffers(uint32_t count, VkCommandBuffer* commandBuffers);
        void freeCommandBuffers(uint32_t count, VkCommandBuffer* commandBuffers);

        VkQueue graphicsQueue() { return m_graphicsQueue; }  // 返回图形队列句柄，用于提交绘制相关命令。
        VkQueue presentQueue() { return m_presentQueue; }  // 返回呈现队列句柄，用于将图像提交到窗口表面。

        VkCommandBuffer beginSingleTimeCommands();  // 开始一次性提交的命令缓冲区，用于执行一次性GPU操作
        void endSingleTimeCommands(VkCommandBuffer commandBuffer);  // 执行完毕一次性命令后释放

        // 通用一次性立即执行的图像布局转换，依据布局转换方式自动配置目前已支持的访问掩码与管线阶段
        void transitionImageLayout(
            VkImage image,
            VkImageLayout oldLayout,
            VkImageLayout newLayout,
            VkAccessFlags srcAccessMask = 0,
            VkAccessFlags dstAccessMask = 0,
            VkImageSubresourceRange subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
            uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, // 队列间移交图像所有权时需要设定
            uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED
        );

        // 创建一个Vulkan缓冲区并为其分配内存
        void createBuffer(
            VkDeviceSize size,
            VkBufferUsageFlags usage,
            VkMemoryPropertyFlags properties,
            VkBuffer& buffer,
            VkDeviceMemory& bufferMemory);

        // 将一个缓冲区中的内容复制到另一个缓冲区
        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

        // 将缓冲区内容拷贝到图像中，常用于上传纹理或贴图数据
        void copyBufferToImage(  
            VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount);

        /**
         * @brief 根据给定的格式信息创建图像资源并分配绑定显存
         * @param format 图像格式
         * @param usageFlags 图像用途
         * @param properties 指定图像内存应具备的属性，例如DEVICE_LOCAL
         * @param image 输出参数，接收创建出来的图像句柄
         * @param imageMemory 输出参数，接收分配好的图像内存句柄
         */
        void createImage(
            uint32_t width,
            uint32_t height,
            VkFormat format,
            VkImageUsageFlags usageFlags,
            VkMemoryPropertyFlags properties,
            VkImage& image,
            VkDeviceMemory& imageMemory) const;

        // 创建纹理采样器
        VkSampler createTextureSampler(
            VkFilter minFilter = VK_FILTER_LINEAR,
            VkFilter maxFilter = VK_FILTER_LINEAR,
            VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT);

    private:
        void createInstance(const char* pAppName);
        void createDebugCallback();
        void createSurface();
        void initPhysicalDevice();
        void createDevice();
        void createCommandPool();

        std::vector<const char*> getRequiredExtensions();   // 获取系统需要的所有拓展名
        void populateDebugUtilsMessengerCreateInfoEXT(VkDebugUtilsMessengerCreateInfoEXT& createInfo);  // 填写验证层关注的信息
        bool checkValidationLayerSupport(); // 检查系统的验证层是否可用

        // 寻找物理设备中符合内存属性要求的内存类型的索引
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;


        void hasGflwRequiredInstanceExtensions();   // 验证需求的拓展是否完善
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device); // 在物理设备中查找创建交换链需要的显示器的能力


    private:
        CzxWindow& m_window;    // 关注的窗口对象
        VkInstance m_instance = VK_NULL_HANDLE;  // 整个应用的入口对象

        const std::vector<const char*> validationLayers = { 
            "VK_LAYER_KHRONOS_validation"
        };  // debug时启用的验证层名称

        const std::vector<const char*> deviceExtensions = {
            VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,    // 动态渲染拓展
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,        // 交换链拓展
            VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME    // 着色器绘制参数拓展
        };  // 需要启用的设备扩展

        VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;  // 调试消息回调句柄，用于输出验证层错误和警告

        VkSurfaceKHR m_surface = VK_NULL_HANDLE;  // 窗口表面

        CzxPhysicalDevices m_physicalDevices{};  // 当前设备存在的所有物理设备及其相关信息属性的抽象
        QueueFamilyIndices m_qFamilyIndices{};

        VkDevice m_device = VK_NULL_HANDLE;  // 逻辑设备

        VkCommandPool m_commandPool = VK_NULL_HANDLE;  // 命令池

        VkQueue m_graphicsQueue = VK_NULL_HANDLE;  // 图形队列句柄
        VkQueue m_presentQueue = VK_NULL_HANDLE;  // 呈现队列句柄


    };

}  // namespace czx