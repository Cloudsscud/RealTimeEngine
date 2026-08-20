#include <czx_utils.h>
#include "czx_device.h"

// std
#include <assert.h>
#include <cstring>  // 提供字符串相关操作函数，方便在Vulkan层检查和复制字符串内容。
#include <iostream>  // 提供标准输出流，便于打印设备信息和调试信息。
#include <set>  // 提供集合容器，便于对队列族和扩展名称进行去重与比较。
#include <unordered_set>  // 提供哈希集合，便于高效判断实例扩展是否已被支持。

namespace czx {

    const char* GetDebugSeverityStr(VkDebugUtilsMessageSeverityFlagBitsEXT Severity) {
        switch (Severity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            return "Verbose";
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            return "Info";
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            return "Warning";
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            return "Error";
        default:
            ERROR("Invalid type code %d", Severity);
        }
        return  "No such Severity!";
    }

    const char* GetDebugType(VkDebugUtilsMessageTypeFlagsEXT Type) {
        switch (Type) {
        case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
            return "General";
        case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
            return "Validation";
        case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
            return "Performance";
        case VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT:
            return "Device address binding";
        default:
            ERROR("Invalid severity code %d", Type);
        }
        return  "No such Type!";
    }

    // 验证层回调函数
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT Severity,     // 传入当前消息的严重级别，用于判断是否需要记录
        VkDebugUtilsMessageTypeFlagsEXT Type,                // 传入当前消息属于哪一类Vulkan消息
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,  // 包含消息正文和相关上下文的回调数据
        void* pUserData) {  // 回调函数可见的数据

        printf("Debug callback: %s\n", pCallbackData->pMessage);
        printf("    Severity %s\n", GetDebugSeverityStr(Severity));
        printf("    Type %s\n", GetDebugType(Type));
        printf("    Objects");

        for (uint32_t i = 0; i < pCallbackData->objectCount; ++i) {
            printf("%llx ", pCallbackData->pObjects[i].objectHandle);
        }
        printf("\n\n");
        return VK_FALSE;  // 不拦截或终止当前消息流程，继续由Vulkan处理
    }

    // 创建调试消息器，便于在启用验证层时接收调试输出。
    VkResult CreateDebugUtilsMessengerEXT(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDebugUtilsMessengerEXT* pDebugMessenger) {

        // 拓展函数需要从instance获取函数地址
        PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessager = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");  // 通过名称获取调试消息器创建函数地址
        if (!vkCreateDebugUtilsMessager) {  // 支持该扩展函数才能创建调试消息器
            ERROR("Cannot find address of vkCreateDebugUtilsMessengerEXT");
        }
        else {
            return vkCreateDebugUtilsMessager(instance, pCreateInfo, pAllocator, pDebugMessenger);
        }
    }

    CzxDevice::CzxDevice(CzxWindow& window) : m_window{ window } {
        createInstance("RealTimeRenderer App");  // 创建并初始化Vulkan实例，建立应用与Vulkan驱动的连接
        createDebugCallback();  // 初始化验证层调试回调
        // surface须在instance创建后，physical device前创建，会有影响
        createSurface();
        initPhysicalDevice();  // 加载所有物理设备并选出可以支持默认队列与呈现的设备及其队列族
        createDevice();
        createCommandPool();  // 创建命令池，后续可以从中分配命令缓冲区
    }

    CzxDevice::~CzxDevice() {
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);  // 释放命令池，后续已经分配的命令缓冲区也会随之失效
        printf("Command pool destroyed\n");

        vkDestroyDevice(m_device, nullptr);  // 销毁逻辑设备，释放其占用的驱动资源
        printf("Device destroyed\n");

        // 销毁窗口表面
        PFN_vkDestroySurfaceKHR vkDestroySurface = (PFN_vkDestroySurfaceKHR)vkGetInstanceProcAddr(m_instance, "vkDestroySurfaceKHR");
        if (!vkDestroySurface) {
            ERROR("Cannot find address of vkDestroySurfaceKHR");
        }
        vkDestroySurface(m_instance, m_surface, nullptr);
        printf("GLFW window surface destroyed\n");


        if (enableValidationLayers) {  // 只有启用了验证层时才需要销毁调试消息器。
            PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessenger = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
            if (!vkDestroyDebugUtilsMessenger) {
                ERROR("Cannot find address of vkDestroyDebugUtilsMessengerEXT");
            }
            vkDestroyDebugUtilsMessenger(m_instance, m_debugMessenger, nullptr);  // 释放调试消息器资源
            printf("Debug callback destroyed\n");
        }


        vkDestroyInstance(m_instance, nullptr);  // 释放Vulkan实例本身占用的底层资源。
        printf("Vulkan instance destroyed\n");
    }

    // 创建Vulkan实例，初始化应用与Vulkan驱动之间的连接，并准备验证层和扩展配置。
    void CzxDevice::createInstance(const char* pAppName) {
        if (enableValidationLayers && !checkValidationLayerSupport()) {  // 先检查所需验证层是否在当前平台上可用。
            ERROR("validation layers requested, but not available!");
        }

        VkApplicationInfo appInfo = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = nullptr,
            .pApplicationName = pAppName,  // 应用名字，便于调试器和驱动展示
            .applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),  // 设置应用版本号，方便识别和调试。
            .pEngineName = "Vulkan Realtime Engine",  // 设置引擎名
            .engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),  // 设置引擎版本号，便于排查兼容性问题。
            .apiVersion = VK_API_VERSION_1_4  // 指定期望使用的Vulkan API版本
        };

        VkInstanceCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,    // 结构体的枚举类型，便于vulkan跳转到相应的处理代码
            .pNext = nullptr,   // 通过链表扩展功能
            .flags = 0,         // 预留未来使用，但必须为0
            .pApplicationInfo = &appInfo,   // optional，app相关信息
            .enabledLayerCount = 0,  // 设置层数量为0，表示不加载任何层
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = 0, // 默认无拓展
            .ppEnabledExtensionNames = nullptr
        };

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;  // 预留调试消息器的创建信息结构体，只有启用验证层时才会使用
        if (enableValidationLayers) {  // 如果启用了验证层，就把相关层和调试回调信息一起注入实例创建信息
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());  // 设置启用的验证层数量
            createInfo.ppEnabledLayerNames = validationLayers.data();  // 传入验证层名称数组，告诉Vulkan需要加载哪些层

            populateDebugUtilsMessengerCreateInfoEXT(debugCreateInfo);  // 填写调试消息器的回调配置，决定收集哪些消息
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;  // 将调试消息器创建信息挂到实例创建链中
        }

        auto extensions = getRequiredExtensions();  // 获取当前平台需要的实例扩展，包含GLFW和调试相关扩展
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
        CHECK_VK_RESULT(result, "Create instance");
        printf("vulkan instance created\n");

        hasGflwRequiredInstanceExtensions();  // 检查并确认实例级扩展已满足后续窗口和调试相关功能的需求。
    }

    void CzxPhysicalDevices::PrintImageUsageFlags(const VkImageUsageFlags& flags) {
        if (flags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
            printf("Image usage transfer src is supported\n");
        }
        if (flags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
            printf("Image usage transfer dst is supported\n");
        }
        if (flags & VK_IMAGE_USAGE_SAMPLED_BIT) {
            printf("Image usage sampled is supported\n");
        }
        if (flags & VK_IMAGE_USAGE_STORAGE_BIT) {
            printf("Image usage storage is supported\n");
        }
        if (flags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
            printf("Image usage color attachment is supported\n");
        }
        if (flags & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            printf("Image usage depth stencil attachment is supported\n");
        }
        if (flags & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT) {
            printf("Image usage transient attachment is supported\n");
        }
        if (flags & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT) {
            printf("Image usage input attachment is supported\n");
        }

        // Todo
        //VK_IMAGE_USAGE_HOST_TRANSFER_BIT = 0x00400000,
        //VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR = 0x00000400,
        //VK_IMAGE_USAGE_VIDEO_DECODE_SRC_BIT_KHR = 0x00000800,
        //VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR = 0x00001000,
        //VK_IMAGE_USAGE_FRAGMENT_DENSITY_MAP_BIT_EXT = 0x00000200,
        //VK_IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR = 0x00000100,
        //VK_IMAGE_USAGE_VIDEO_ENCODE_DST_BIT_KHR = 0x00002000,
        //VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR = 0x00004000,
        //VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR = 0x00008000,
        //VK_IMAGE_USAGE_ATTACHMENT_FEEDBACK_LOOP_BIT_EXT = 0x00080000,
        //VK_IMAGE_USAGE_INVOCATION_MASK_BIT_HUAWEI = 0x00040000,
        //VK_IMAGE_USAGE_SAMPLE_WEIGHT_BIT_QCOM = 0x00100000,
        //VK_IMAGE_USAGE_SAMPLE_BLOCK_MATCH_BIT_QCOM = 0x00200000,
        //VK_IMAGE_USAGE_TENSOR_ALIASING_BIT_ARM = 0x00800000,
        //VK_IMAGE_USAGE_TILE_MEMORY_BIT_QCOM = 0x08000000,
    }
    void CzxPhysicalDevices::PrintMemoryProperty(const VkMemoryPropertyFlags& flags) {
        if (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
            printf("DEVICE LOCAL");
        }
        if (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            printf("HOST VISIBLE");
        }
        if (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) {
            printf("HOST COHERENT");
        }
        if (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) {
            printf("HOST CACHED");
        }
        if (flags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) {
            printf("LAZILY ALLOCATED");
        }
        if (flags & VK_MEMORY_PROPERTY_PROTECTED_BIT) {
            printf("PROTECTED");
        }
        if (flags & VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD) {
            printf("DEVICE COHERENT");
        }
        if (flags & VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD) {
            printf("DEVICE UNCACHED");
        }
        if (flags & VK_MEMORY_PROPERTY_RDMA_CAPABLE_BIT_NV) {
            printf("RDMA CAPABLE");
        }
    }

    // 从候选格式中选择物理设备真正支持的格式
    static VkFormat findSupportedFormat(VkPhysicalDevice physDevice,
        const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
        for (VkFormat format : candidates) { 
            // 依次检查每个候选格式是否满足要求
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physDevice, format, &props);  // 查询当前设备对该格式的具体支持特性

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {  // 线性平铺
                return format;
            }
            else if (
                tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {  // 最优平铺
                return format;
            }
        }
        ERROR("failed to find supported format!\n");  // 所有候选格式都不满足
    }

    // 查找当前设备支持的深度格式，供深度附件创建时使用。
    static VkFormat findDepthFormat(VkPhysicalDevice physDevice) {
        std::vector<VkFormat> candidates = {
             VK_FORMAT_D32_SFLOAT,
             VK_FORMAT_D32_SFLOAT_S8_UINT,
             VK_FORMAT_D24_UNORM_S8_UINT
        };
        return findSupportedFormat(physDevice,
            candidates,  // 依次尝试几种常见的深度格式
            VK_IMAGE_TILING_OPTIMAL,  // 使用最优平铺，适合深度附件
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);  // 只选择支持深度模板附件用途的格式
    }

    void CzxPhysicalDevices::Init(const VkInstance& instance, const VkSurfaceKHR& surface) {
        uint32_t deviceCount = 0;  // 记录当前系统中可枚举到的物理设备数量

        VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);  // 先获取设备数量，便于分配容器
        CHECK_VK_RESULT(result, "vkEnumeratePhysicalDevices error (1)\n");

        std::cout << "Physical Device count: " << deviceCount << std::endl;  // 设备数量

        m_devices.resize(deviceCount);  // 更新物理设备信息数组大小

        // 栈上的物理设备句柄数组
        std::vector<VkPhysicalDevice> Devices;
        Devices.resize(deviceCount);
        result = vkEnumeratePhysicalDevices(instance, &deviceCount, Devices.data());  // 把实际枚举出的设备句柄写入数组
        CHECK_VK_RESULT(result, "vkEnumeratePhysicalDevices error (2)\n");

        // 存入对应下标的设备信息结构体中
        for (uint32_t i = 0; i < deviceCount; ++i) {
            VkPhysicalDevice physDevice = Devices[i];
            m_devices[i].m_physDevice = physDevice; // 更新句柄

            vkGetPhysicalDeviceProperties(physDevice, &m_devices[i].m_devProps);    // 更新设备属性
            printf("Device Name: %s\n", m_devices[i].m_devProps.deviceName);
            uint32_t apiVer = m_devices[i].m_devProps.apiVersion;
            printf("    API version: %d.%d.%d.%d\n",
                VK_API_VERSION_VARIANT(apiVer),
                VK_API_VERSION_MAJOR(apiVer),
                VK_API_VERSION_MINOR(apiVer),
                VK_API_VERSION_PATCH(apiVer));

            // 更新设备支持的队列家族
            uint32_t qFamiliesCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &qFamiliesCount, nullptr);
            printf("Queue Familiy Count: %d\n", qFamiliesCount);

            m_devices[i].m_qFamilyProps.resize(qFamiliesCount);
            m_devices[i].m_qSupportsPresent.resize(qFamiliesCount);

            vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &qFamiliesCount, m_devices[i].m_qFamilyProps.data());

            // 检查队列家族的属性
            for (uint32_t q = 0; q < qFamiliesCount; ++q) {
                const VkQueueFamilyProperties& QFamilyProps = m_devices[i].m_qFamilyProps[q];

                printf("    Family %d Queue Count: %d ", q, QFamilyProps.queueCount);
                // 队列支持的功能标记
                VkQueueFlags qFlags = QFamilyProps.queueFlags;
                printf("    GFX %s, Compute %s, Transfer %s, Sparse binding %s\n",
                    (qFlags & VK_QUEUE_GRAPHICS_BIT) ? "Yes" : "No",
                    (qFlags & VK_QUEUE_COMPUTE_BIT) ? "Yes" : "No",
                    (qFlags & VK_QUEUE_TRANSFER_BIT) ? "Yes" : "No",
                    (qFlags & VK_QUEUE_SPARSE_BINDING_BIT) ? "Yes" : "No");

                result = vkGetPhysicalDeviceSurfaceSupportKHR(physDevice, q, surface, &m_devices[i].m_qSupportsPresent[q]);
                CHECK_VK_RESULT(result, "vkGetPhysicalDeviceSurfaceSupportKHR error\n");
            }

            // 更新支持的表面格式
            uint32_t formatsCount = 0;
            result = vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice, surface, &formatsCount, nullptr);
            CHECK_VK_RESULT(result, "vkGetPhysicalDeviceSurfaceFormatsKHR error (1)\n");
            assert(formatsCount > 0 && "SurfaceFormatsCount error");

            m_devices[i].m_surfaceFormats.resize(formatsCount);

            result = vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice, surface, &formatsCount, m_devices[i].m_surfaceFormats.data());
            CHECK_VK_RESULT(result, "vkGetPhysicalDeviceSurfaceFormatsKHR error (2)\n");

            // 展示支持的表面格式信息
            for (uint32_t f = 0; f < formatsCount; ++f) {
                const VkSurfaceFormatKHR& surfaceFormat = m_devices[i].m_surfaceFormats[f];
                printf("    Format %x Color space %x\n", surfaceFormat.format, surfaceFormat.colorSpace);
            }

            // 更新表面特性信息
            result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDevice, surface, &m_devices[i].m_surfaceCaps);
            CHECK_VK_RESULT(result, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR error\n");

            PrintImageUsageFlags(m_devices[i].m_surfaceCaps.supportedUsageFlags);

            // 更新支持的表面呈现模式
            uint32_t presentModesCount = 0;
            result = vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice, surface, &presentModesCount, nullptr);
            CHECK_VK_RESULT(result, "vkGetPhysicalDeviceSurfacePresentModesKHR error (1)\n");
            assert(presentModesCount > 0 && "SurfacePresentModesCount error");

            m_devices[i].m_presentModes.resize(presentModesCount);

            result = vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice, surface, &presentModesCount, m_devices[i].m_presentModes.data());
            CHECK_VK_RESULT(result, "vkGetPhysicalDeviceSurfacePresentModesKHR error (2)\n");
            printf("Present Modes Count %d ", presentModesCount);

            // 更新内存属性
            vkGetPhysicalDeviceMemoryProperties(physDevice, &m_devices[i].m_memProps);
            printf("Memory Types Count %d\n", m_devices[i].m_memProps.memoryTypeCount);
            for (uint32_t m = 0; m < m_devices[i].m_memProps.memoryTypeCount; ++m) {
                printf("%d: flags %x heap %d ", 
                    m,
                    m_devices[i].m_memProps.memoryTypes[m].propertyFlags,
                    m_devices[i].m_memProps.memoryTypes[m].heapIndex);

                PrintMemoryProperty(m_devices[i].m_memProps.memoryTypes[m].propertyFlags);
                printf("\n");
            }
            printf("Heap Type Count %d\n", m_devices[i].m_memProps.memoryHeapCount);
            printf("\n");

            // 更新拥有的功能
            vkGetPhysicalDeviceFeatures(physDevice, &m_devices[i].m_features);

            m_devices[i].m_depthFormat = findDepthFormat(m_devices[i].m_physDevice);
        }
    }

    // 检查是否有设备满足需求:含需求的队列、是否支持显示功能
    QueueFamilyIndices CzxPhysicalDevices::SelectDevice(VkQueueFlags RequiredQueueType, bool SupportsPresent) {

        for (uint32_t i = 0; i < m_devices.size(); ++i) {
            // 对于每个设备找到其支持的队列族集合
            QueueFamilyIndices indices;

            for (uint32_t q = 0; q < m_devices[i].m_qFamilyProps.size(); ++q) {
                // 对于每个队列族，找到满足所有需求那个设备
                const VkQueueFamilyProperties& queueFamily = m_devices[i].m_qFamilyProps[q];

                if (queueFamily.queueFlags & RequiredQueueType) {  // 如果队列族中存在可用队列且支持该能力，就记录该队列族索引
                    indices.graphicsFamily = q;
                }
                if (((bool)m_devices[i].m_qSupportsPresent[q] == SupportsPresent)) {  // 如果队列族存在并且支持呈现，就记录它。
                    indices.presentFamily = q;  // 保存呈现队列族索引
                }
                if (indices.isComplete()) {  // 如果图形和呈现队列族都已找到，就无需再继续枚举
                    m_devIndex = i;
                    printf("Using GFX Device %d, Queue Family %d, Present Family %d\n", m_devIndex, indices.graphicsFamily.value(), indices.presentFamily.value());
                    return indices;  // 返回找到的图形和呈现队列族索引
                }
                else {
                    continue;
                }
            }
        }
        ERROR("Required queue type %x and supports present %d not found\n", RequiredQueueType, SupportsPresent);
        return {};
    }
    // 获取的那个满足需求的设备
    const PhysicalDevice& CzxPhysicalDevices::Selected() const {
        if (m_devIndex < 0) {
            ERROR("A physical has not been selected\n");
        }
        return m_devices[m_devIndex];
    }


    // 初始化物理设备并选出有能呈现的图形队列的设备
    void CzxDevice::initPhysicalDevice() {
        m_physicalDevices.Init(m_instance, m_surface);
        m_qFamilyIndices = m_physicalDevices.SelectDevice(VK_QUEUE_GRAPHICS_BIT, true);
    }



    // 逻辑设备创建
    void CzxDevice::createDevice() {
        float qPriorities[] = { 1.f, 1.f};  // 队列优先级数组,0.0->1.0

        // 队列创建信息，用于逻辑设备管理各个队列族的各个队列
        VkDeviceQueueCreateInfo qInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .queueFamilyIndex = m_qFamilyIndices.graphicsFamily.value(),  // 需求的那个队列族在选中的物理设备里的索引
            .queueCount = 2,    // 该队列家族使用的队列的数量
            .pQueuePriorities = &qPriorities[0]   // 说明队列族中使用的各个队列的优先级
        };

        // 启用功能之前需要先检查是否支持这些功能
        if (m_physicalDevices.Selected().m_features.samplerAnisotropy == VK_FALSE) {
            ERROR("The Sampler Anisotropy is not supported!\n");
        }
        if (m_physicalDevices.Selected().m_features.geometryShader == VK_FALSE) {
            ERROR("The Geometry Shader is not supported!\n");
        }
        if (m_physicalDevices.Selected().m_features.tessellationShader == VK_FALSE) {
            ERROR("The Tessellation Shader is not supported!\n");
        }

        VkPhysicalDeviceFeatures deviceFeatures = { 0 };  // 当前逻辑设备所需的功能
        deviceFeatures.samplerAnisotropy = VK_TRUE;  // 开启各向异性采样支持，后续纹理采样更真实。
        deviceFeatures.geometryShader = VK_TRUE;    // 几何着色器
        deviceFeatures.tessellationShader = VK_TRUE;    // 细分着色器

        // 启用动态渲染
        VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeature = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
            .pNext = nullptr,
            .dynamicRendering = VK_TRUE
        };

        // 物理设备创建信息
        VkDeviceCreateInfo deviceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = &dynamicRenderingFeature,
            .flags = 0,
            .queueCreateInfoCount = 1,  // 使用的队列族
            .pQueueCreateInfos = &qInfo,
            .enabledLayerCount = 0, // 验证层不启用
            .ppEnabledLayerNames = nullptr, // 验证层不启用
            .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),  // 设置设备扩展数量
            .ppEnabledExtensionNames = deviceExtensions.data(),  // 需求的拓展名
            .pEnabledFeatures = &deviceFeatures // 逻辑设备应该拥有的功能
        };

        VkResult result = vkCreateDevice(m_physicalDevices.Selected().m_physDevice, &deviceCreateInfo, nullptr, &m_device);
        CHECK_VK_RESULT(result, "create device");
        printf("\nDevice Created\n");

        vkGetDeviceQueue(m_device, m_qFamilyIndices.graphicsFamily.value(), 0, &m_graphicsQueue);  // 获取图形队列句柄
        vkGetDeviceQueue(m_device, m_qFamilyIndices.graphicsFamily.value(), 1, &m_presentQueue);  // 获取呈现队列句柄
    }

    // 创建命令池，后续用于分配命令缓冲区执行一次性或循环提交的GPU命令
    void CzxDevice::createCommandPool() {

        // 从成员函数中查找适合创建命令池的队列族
        VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,  // 设置命令池的用途标志，方便短命令和重置命令缓冲区
        .queueFamilyIndex = m_qFamilyIndices.graphicsFamily.value()  // 指定命令池关联的图形队列族
        };

        VkResult result = vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool);
        CHECK_VK_RESULT(result, "create command pool!\n");
    }

    void CzxDevice::createCommandBuffers(uint32_t count, VkCommandBuffer* commandBuffers) {
        VkCommandBufferAllocateInfo allocateInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = m_commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,   // 主命令缓冲区
            .commandBufferCount = count,    // 分配的命令缓冲区数量
        };

        VkResult result = vkAllocateCommandBuffers(m_device, &allocateInfo, commandBuffers);
        CHECK_VK_RESULT(result, "allocate command buffers");
        printf("%d command buffers allocated!\n", count);
    }

    // 释放命令缓冲区资源，通常在渲染器销毁或重建时调用
    void CzxDevice::freeCommandBuffers(uint32_t count, VkCommandBuffer* commandBuffers) {
        vkFreeCommandBuffers(m_device, m_commandPool, count, commandBuffers);
        printf("CommandBuffers freed\n");
    }

    // 调用窗口对象为当前Vulkan实例创建窗口表面，以便后续交换链和呈现流程使用。
    void CzxDevice::createSurface() {  // 这个函数把当前窗口和Vulkan实例连接起来，形成可用于呈现的表面。
        m_window.createWindowSurface(m_instance, &m_surface);  // 通过窗口对象把表面句柄写入成员变量。
    }

    // 配置调试消息器的回调参数，决定接收哪些严重级别和消息类型的输出。
    void CzxDevice::populateDebugUtilsMessengerCreateInfoEXT(
        VkDebugUtilsMessengerCreateInfoEXT& createInfo) {

        createInfo = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .pNext = nullptr,
            // 消息严重级别
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |    // 详细
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |       // 提示
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |    // 警告
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,       // 错误
            // 感兴趣的消息类型
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |        // 通用信息
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |     // 验证相关
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,     // 性能相关
            .pfnUserCallback = &debugCallback, // 调试回调函数
            .pUserData = nullptr    // 指向任意想在回调函数接受的内容
        };
    }

    // 初始化验证层调试消息器，便于在开发阶段定位Vulkan调用错误。
    void CzxDevice::createDebugCallback() {
        if (!enableValidationLayers) return;  // 没有启用验证层时，不需要创建调试消息器

        VkDebugUtilsMessengerCreateInfoEXT createInfo;  // 先准备调试消息器的创建信息结构体
        populateDebugUtilsMessengerCreateInfoEXT(createInfo);  // 填写消息器回调相关配置

        VkResult result = CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger);
        CHECK_VK_RESULT(result, "debug utils messenger");
    }

    // 检查当前系统是否提供了所需的验证层，以保证调试功能可用。
    bool CzxDevice::checkValidationLayerSupport() {  // 这个函数用于确认当前环境支持所有需要的验证层。
        uint32_t layerCount;  // 保存实例层属性枚举结果中的层数量。
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);  // 先枚举可用验证层数量。

        std::vector<VkLayerProperties> availableLayers(layerCount);  // 按数量分配可用层数组。
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());  // 把实际可用的验证层信息写入数组。

        for (const char* layerName : validationLayers) {  // 依次检查项目要求的每个验证层是否存在。
            bool layerFound = false;  // 默认认为当前层未找到，随后在列表中查找。

            for (const auto& layerProperties : availableLayers) {  // 遍历当前系统支持的所有验证层。
                if (strcmp(layerName, layerProperties.layerName) == 0) {  // 只要名字匹配，就认为该层可用。
                    layerFound = true;  // 标记该层已找到。
                    break;  // 找到后可以提前停止当前内层循环。
                }
            }

            if (!layerFound) {  // 若任意一层未找到，就说明当前系统无法满足要求。
                return false;  // 直接返回false，表示验证层支持检查失败。
            }
        }

        return true;  // 所有需要的验证层都已找到，返回true。
    }

    // 获取当前平台下创建Vulkan实例所需的扩展列表，包含GLFW和调试相关扩展。
    std::vector<const char*> CzxDevice::getRequiredExtensions() {  // 这个函数负责收集Vulkan实例创建所需的扩展名称。
        uint32_t glfwExtensionCount = 0;  // 保存GLFW返回的扩展数量。
        const char** glfwExtensions;  // 声明GLFW返回的扩展名称数组指针。
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);  // 从GLFW拿到当前平台所需的实例扩展。

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);  // 把GLFW返回的扩展名拷贝到vector中，方便后续传递。

        if (enableValidationLayers) {  // 只有启用调试验证层时，才需要额外添加调试扩展。
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);  // 将调试消息器扩展名加到扩展列表中。
        }

        return extensions;  // 返回最终组装好的扩展列表，供实例创建使用。
    }

    // 检查实例扩展是否齐全，避免后续创建窗口表面或调试回调时因为缺少扩展而失败。
    void CzxDevice::hasGflwRequiredInstanceExtensions() {  // 这个函数负责验证当前环境是否真正支持所有实例扩展。
        uint32_t extensionCount = 0;  // 保存实例扩展枚举得到的数量。
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);  // 先查询扩展数量，便于分配数组。
        std::vector<VkExtensionProperties> extensions(extensionCount);  // 创建扩展信息数组。
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());  // 把实际扩展属性写入数组。

        std::cout << "available extensions:" << std::endl;  // 输出当前系统可用扩展列表的标题。
        std::unordered_set<std::string> available;  // 使用哈希集合记录已枚举出的扩展名，便于快速查找。
        for (const auto& extension : extensions) {  // 遍历所有可用扩展并输出它们的名称。
            std::cout << "\t" << extension.extensionName << std::endl;  // 将扩展名按缩进形式打印出来，便于查看。
            available.insert(extension.extensionName);  // 把扩展名放入集合，供后续快速匹配。
        }

        std::cout << "required extensions:" << std::endl;  // 输出当前项目所需扩展列表标题。
        auto requiredExtensions = getRequiredExtensions();  // 重新拿到当前项目所需的扩展集合。
        for (const auto& required : requiredExtensions) {  // 遍历每个必须存在的扩展。
            std::cout << "\t" << required << std::endl;  // 输出扩展名，便于观察和排错。
            if (available.find(required) == available.end()) {  // 如果该扩展不在当前系统可用集合中，就说明缺失。
                throw std::runtime_error("Missing required glfw extension");  // 抛出异常，明确提示扩展缺失。
            }
        }
    }

    // 检查物理设备是否支持交换链所需的扩展。
    bool CzxDevice::checkDeviceExtensionSupport(VkPhysicalDevice device) {  // 这个函数负责确认物理设备支持当前项目所需的设备扩展。
        uint32_t extensionCount;  // 保存设备扩展枚举得到的数量。
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);  // 先查询设备扩展数量。

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);  // 按数量创建可用扩展数组。
        vkEnumerateDeviceExtensionProperties(
            device,  // 传入当前物理设备句柄，枚举该设备支持的扩展。
            nullptr,  // 设备层扩展属性通常不需要指定层。
            &extensionCount,  // 传入数量地址，接收实际枚举出的扩展数。
            availableExtensions.data());  // 把扩展信息写入数组。

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());  // 使用集合存储当前项目所需的设备扩展名称。

        for (const auto& extension : availableExtensions) {  // 遍历当前设备实际支持的每个扩展。
            requiredExtensions.erase(extension.extensionName);  // 如果当前设备支持该扩展，就从所需集合中移除它。
        }

        return requiredExtensions.empty();  // 若所需集合已空，说明当前设备支持全部必需扩展。
    }

    // 查询当前物理设备对交换链的能力支持情况，包括格式、呈现模式和表面能力。
    SwapChainSupportDetails CzxDevice::querySwapChainSupport(VkPhysicalDevice device) {  // 这个函数用于收集交换链创建时所需的能力信息。
        SwapChainSupportDetails details;  // 创建一个结构体，用于容纳查询结果。
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);  // 获取当前表面支持的基础能力，交换链中图像数量、图像最小/最大尺寸。

        uint32_t formatCount;  // 保存表面格式数量。
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);  // 先查询可用表面格式数量。

        if (formatCount != 0) {  // 只有存在可用格式时才分配并填写数组。
            details.formats.resize(formatCount);  // 根据数量调整存储容器大小。
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());  // 把真实表面格式数据写入结果中。
        }

        uint32_t presentModeCount;  // 保存可用呈现模式数量。
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);  // 先查询可用呈现模式数量。

        if (presentModeCount != 0) {  // 只有存在可用呈现模式时才真正获取数据。
            details.presentModes.resize(presentModeCount);  // 根据数量调整呈现模式容器大小。
            vkGetPhysicalDeviceSurfacePresentModesKHR(
                device,  // 传入当前物理设备句柄。
                m_surface,  // 当前窗口表面句柄。
                &presentModeCount,  // 传入数量地址，接收实际可用数量。
                details.presentModes.data());  // 把可用呈现模式写入结果容器。
        }
        return details;  // 返回整理好的交换链支持信息，供后续选择合适的格式和模式。
    }

    // 开始一次性提交的命令缓冲区，用于执行临时性的GPU命令
    VkCommandBuffer CzxDevice::beginSingleTimeCommands() {
        // 单命令配置信息
        VkCommandBufferAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext = nullptr,
            .commandPool = m_commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext = nullptr,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT    // 命令缓冲区只提交一次后销毁，适合临时操作
        };

        vkBeginCommandBuffer(commandBuffer, &beginInfo);  // 开启命令缓冲区记录，后续一个命令会写入其中
        return commandBuffer;
    }

    // 结束一次性命令缓冲区并提交到图形队列执行，随后释放命令缓冲区资源
    void CzxDevice::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
        vkEndCommandBuffer(commandBuffer);  // 结束命令缓冲区的录制，可以提交

        VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .pWaitDstStageMask = 0,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer,
            .signalSemaphoreCount = 0,
            .pSignalSemaphores = nullptr
        };

        vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);  // 把命令缓冲区提交到图形队列执行
        vkQueueWaitIdle(m_graphicsQueue);  // 等待图形队列空闲，确保一次性命令立即执行完成

        vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);  // 执行完毕后把命令缓冲区释放回命令池
    }

    // 根据内存类型筛选条件和所需属性，找到适合当前资源的内存类型
    uint32_t CzxDevice::findMemoryType(uint32_t memoryTypeBits, VkMemoryPropertyFlags properties) const{

        VkPhysicalDeviceMemoryProperties memProps = m_physicalDevices.Selected().m_memProps;  // 设备可用内存类型的信息

        for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
            // 遍历所有内存类型，寻找符合条件的那一个
            if ((memoryTypeBits & (1 << i)) // 检查当前索引的内存类型是否支持
                && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {  // 检查当前索引对应的内存类型的属性是否满足需求
                return i;  // 返回对应的内存类型索引
            }
        }

        ERROR("failed to find suitable memory type!\n");  // 没有找到合适内存类型
        return -1;
    }

    /**
     * @brief 借助逻辑设备创建一个Vulkan缓冲区并分配对应内存
     * @param size          需要创建的缓冲区大小，单位是字节
     * @param usage         指定缓冲区将要用于哪些Vulkan用途，比如vertex_buffer或transfer_src|dst
     * @param properties    指定缓冲区内存应具备的属性，例如HOST_VISIBLE或DEVICE_LOCAL
     * @param buffer        输出参数，接收创建出来的缓冲区句柄
     * @param bufferMemory  输出参数，接收分配好的显存或内存句柄
     */
    void CzxDevice::createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& bufferMemory) {

        VkBufferCreateInfo bufferInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = size,       // 缓冲区字节大小
            .usage = usage,     // 缓冲区用途
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE    // 独占缓冲区,只由一个队列使用
        };

        VkResult result = vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer);
        CHECK_VK_RESULT(result, "create buffer\n");
        printf("buffer created:\n");

        VkMemoryRequirements memRequirements;  // 创建内存需求结构体，用于获取缓冲区所需的内存属性
        vkGetBufferMemoryRequirements(m_device, buffer, &memRequirements);  // 查询当前缓冲区所需的内存大小和类型位掩码
        printf("    memory requires %d bytes\n", (int)memRequirements.size);

        uint32_t memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);
        printf("    memory type index %d\n", memoryTypeIndex);

        VkMemoryAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memRequirements.size,     //缓冲区分配大小
            .memoryTypeIndex = memoryTypeIndex   // 最合适的内存类型索引
        };

        result = vkAllocateMemory(m_device, &allocInfo, nullptr, &bufferMemory);
        CHECK_VK_RESULT(result, "allocate buffer memory\n");
        printf("buffer memory allocated\n");

        result = vkBindBufferMemory(m_device, buffer, bufferMemory, 0);  // 把刚刚分配的内存绑定到缓冲区上，形成可用的缓冲区资源
        CHECK_VK_RESULT(result, "bind buffer with memory\n");
    }

    // 用一次性命令把源缓冲区内容复制到目标缓冲区。
    void CzxDevice::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferCopy copyRegion{
            .srcOffset = 0,     // 复制时源缓冲区偏移量为0，表示从起点开始复制
            .dstOffset = 0,     // 复制时目标缓冲区偏移量为0，表示写入目标起点
            .size = size        // 设置本次拷贝的字节数量
        }; 
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);  // 把命令记录进一次性命令缓冲区

        endSingleTimeCommands(commandBuffer);  // 提交并等待执行
    }

    /**
     * @brief  把缓冲区内容拷贝到图像中
     * @param buffer 源缓冲区句柄，包含要写入图像的数据
     * @param image 目标图像句柄，接收缓冲区内容
     * @param width 目标图像的宽度
     * @param height 目标图像的高度
     * @param layerCount default=1 目标图像的层数
     */
    void CzxDevice::copyBufferToImage(VkBuffer buffer,VkImage image,uint32_t width,uint32_t height,uint32_t layerCount) {

        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferImageCopy region{
            .bufferOffset = 0,// 从源缓冲区的起始位置开始复制
            .bufferRowLength = 0,// 不进行行间距补齐，按连续排布处理
            .bufferImageHeight = 0,// 不进行图像高度补齐，按连续排布处理
            .imageSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,  // 说明当前复制的是颜色通道内容
                .mipLevel = 0,  // 指向mip 0级别的纹理内容
                .baseArrayLayer = 0,  // 从第0层数组开始复制
                .layerCount = layerCount  // 指定复制的层数
            },
            .imageOffset = { 0, 0, 0 }, // 目标图像复制起始偏移量设为左上角原点
            .imageExtent = { width, height, 1 }// 设置目标图像的复制尺寸，深度为1
        };

        vkCmdCopyBufferToImage(
            commandBuffer,  // 传入当前正在记录的命令缓冲区。
            buffer,  // 源缓冲区句柄。
            image,  // 目标图像句柄。
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,  // 指定目标图像当前布局满足传输拷贝需求。
            1,  // 只有一个拷贝区域。
            &region);  // 传入该区域描述结构体。
        endSingleTimeCommands(commandBuffer);  // 提交并等待拷贝执行完成。
    }

    void CzxDevice::createImage(
        uint32_t width,
        uint32_t height,
        VkFormat format,
        VkImageUsageFlags usageFlags,
        VkMemoryPropertyFlags properties,  
        VkImage& image,
        VkDeviceMemory& imageMemory) const {

        // 创建图像
        VkImageCreateInfo imageInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,  // 2D mipmap层级
            .format = format,
            .extent = {
                .width = width,
                .height = height,
                .depth = 1  // 深度维度为1，表示二维平面
            },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,   // 单像素采样数为1，关闭多重采样
            .tiling = VK_IMAGE_TILING_OPTIMAL,  // 使用最优平铺，适合设备内存和附件使用
            .usage = usageFlags,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,   // 独占
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED  // 初始布局未定义，渲染前需转换
        };

        VkResult result = vkCreateImage(m_device, &imageInfo, nullptr, &image);
        CHECK_VK_RESULT(result, "create image");
        printf("image created:\n");

        VkMemoryRequirements memRequirements;  // 创建内存需求结构体，用于查询图像所需的内存规格。
        vkGetImageMemoryRequirements(m_device, image, &memRequirements);  // 计算此图像需要的内存大小和类型位掩码
        printf("image requires %d bytes\n", (int)memRequirements.size);

        VkMemoryAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memRequirements.size, // 设置需要分配的内存大小
            .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)   // 选择适合当前图像的内存类型索引
        };

        result = vkAllocateMemory(m_device, &allocInfo, nullptr, &imageMemory);
        CHECK_VK_RESULT(result, "allocate image memory\n");
        
        result = vkBindImageMemory(m_device, image, imageMemory, 0);
        CHECK_VK_RESULT(result, "bind image memory\n");
    }

    VkImageView CzxDevice::createImageView(
        VkImage image, VkFormat format,
        VkImageAspectFlags aspectMask,
        VkImageViewType viewType,
        uint32_t layerCount, uint32_t mipLevels){

        VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = image,
        .viewType = viewType,
        .format = format,
        .components = {     // 组件属性保留原通道的顺序
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = {
                .aspectMask = aspectMask,
                .baseMipLevel = 0,
                .levelCount = mipLevels,
                .baseArrayLayer = 0,
                .layerCount = layerCount
        }
        };

        VkImageView imageView;
        VkResult result = vkCreateImageView(m_device, &viewInfo, nullptr, &imageView);
        CHECK_VK_RESULT(result ,"create image view!");
        
        return imageView;
    }

    VkSampler CzxDevice::createTextureSampler(VkFilter minFilter, VkFilter maxFilter, VkSamplerAddressMode addressMode) {
        VkSamplerCreateInfo samplerInfo{
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .magFilter = minFilter,
            .minFilter = maxFilter,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,    // 线性mipmap
            .addressModeU = addressMode,    // 三坐标轴相同的寻址方式
            .addressModeV = addressMode,
            .addressModeW = addressMode,
            .mipLodBias = 0.0f, // 无lod偏差
            .anisotropyEnable = VK_FALSE,
            .maxAnisotropy = 1,
            .compareEnable = VK_FALSE,
            .compareOp = VK_COMPARE_OP_ALWAYS,
            .minLod = 0.0f,
            .maxLod = 0.0f,
            .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
            .unnormalizedCoordinates = VK_FALSE,
        };

        VkSampler sampler;
        VkResult result = vkCreateSampler(m_device, &samplerInfo, nullptr, &sampler);
        CHECK_VK_RESULT(result, "create sampler");
        return sampler;
    }

    void CzxDevice::transitionImageLayout(
        VkImage image,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        VkAccessFlags srcAccessMask,
        VkAccessFlags dstAccessMask,
        VkImageSubresourceRange subresourceRange,
        uint32_t srcQueueFamilyIndex,
        uint32_t dstQueueFamilyIndex
    ) {

        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkImageMemoryBarrier barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = srcAccessMask,
            .dstAccessMask = dstAccessMask,
            .oldLayout = oldLayout,
            .newLayout = newLayout,
            .srcQueueFamilyIndex = srcQueueFamilyIndex,
            .dstQueueFamilyIndex = dstQueueFamilyIndex,
            .image = image,
            .subresourceRange = subresourceRange
        };

        VkPipelineStageFlags srcStage;   // 最后一次管线写入阶段
        VkPipelineStageFlags dstStage;  // 管线读取阶段

        switch (oldLayout) {
        case VK_IMAGE_LAYOUT_UNDEFINED: {
            barrier.srcAccessMask = 0;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

            if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            }
            else if (newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            }
            else if (newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
                barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            }
            else if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
                barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            }
            break;
        }

        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL: {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

            if (newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            }
            else if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            }
            else if (newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
                barrier.dstAccessMask = 0; // 呈现不需要访问掩码
                dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            }
            break;
        }

        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL: {
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

            if (newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            }
            break;
        }

        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL: {
            barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

            if (newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            }
            else if (newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
                barrier.dstAccessMask = 0;
                dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            }
            break;
        }

        default: {
            ERROR("Unsupported layout transition!");
        }
        }

        vkCmdPipelineBarrier(
            commandBuffer,
            srcStage, dstStage,
            0,  // 依赖标志
            0, nullptr, // memory barriers
            0, nullptr, // buffer memory barriers
            1, &barrier // image memory barriers
        );

        endSingleTimeCommands(commandBuffer);
    }

}  // namespace lve