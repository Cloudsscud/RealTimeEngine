#include "czx_device.h"

// std
#include <cstring>  // 提供字符串相关操作函数，方便在Vulkan层检查和复制字符串内容。
#include <iostream>  // 提供标准输出流，便于打印设备信息和调试信息。
#include <set>  // 提供集合容器，便于对队列族和扩展名称进行去重与比较。
#include <unordered_set>  // 提供哈希集合，便于高效判断实例扩展是否已被支持。

namespace czx {

    // 验证层回调函数，用于在调试阶段输出Vulkan验证层发出的警告或错误信息。
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,  // 传入当前消息的严重级别，用于判断是否需要记录。
        VkDebugUtilsMessageTypeFlagsEXT messageType,  // 传入当前消息属于哪一类Vulkan消息，便于区分验证、性能或通用消息。
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,  // 包含消息正文和相关上下文的回调数据。
        void* pUserData) {  // 保留用户自定义数据指针，当前实现中未使用。
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;  // 把验证层暴露出来的消息输出到控制台，方便定位问题。

        return VK_FALSE;  // 返回false表示不拦截或终止当前消息流程，继续由Vulkan处理。
    }

    // 通过实例函数地址创建调试消息器，便于在启用验证层时接收调试输出。
    VkResult CreateDebugUtilsMessengerEXT(
        VkInstance instance,  // 当前Vulkan实例句柄，用于从实例级函数表中获取调试扩展函数。
        const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,  // 指向调试消息器的创建信息结构体。
        const VkAllocationCallbacks* pAllocator,  // 可选的内存分配回调，用于控制扩展对象的内存分配。
        VkDebugUtilsMessengerEXT* pDebugMessenger) {  // 输出参数，用于接收创建出来的调试消息器句柄。
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            instance,  // 传入实例句柄，获取实例级的扩展函数地址。
            "vkCreateDebugUtilsMessengerEXT");  // 通过名称获取调试消息器创建函数地址。
        if (func != nullptr) {  // 说明当前系统支持该扩展函数，才能创建调试消息器。
            return func(instance, pCreateInfo, pAllocator, pDebugMessenger);  // 调用扩展函数完成消息器的实际创建。
        }
        else {  // 如果扩展函数不存在，说明当前环境不支持调试消息器。
            return VK_ERROR_EXTENSION_NOT_PRESENT;  // 返回对应错误码，提示扩展不可用。
        }
    }

    // 通过实例函数地址销毁调试消息器，释放验证层相关回调资源。
    void DestroyDebugUtilsMessengerEXT(
        VkInstance instance,  // 当前Vulkan实例句柄，用于定位调试扩展函数。
        VkDebugUtilsMessengerEXT debugMessenger,  // 需要销毁的调试消息器句柄。
        const VkAllocationCallbacks* pAllocator) {  // 可选的内存分配回调，通常为空。
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            instance,  // 传入实例句柄，获取销毁函数地址。
            "vkDestroyDebugUtilsMessengerEXT");  // 用函数名获取调试消息器销毁函数地址。
        if (func != nullptr) {  // 只有扩展函数存在时才进行销毁。
            func(instance, debugMessenger, pAllocator);  // 调用扩展函数释放调试消息器资源。
        }
    }

    // class member functions
    // 构造函数依次完成Vulkan实例、调试消息器、窗口表面、物理设备、逻辑设备和命令池的初始化。
    CzxDevice::CzxDevice(CzxWindow& window) : m_window{ window } {  // 构造函数把窗口对象引用保存到成员中，后续创建表面和设备都依赖它。
        createInstance();  // 创建并初始化Vulkan实例，建立应用与Vulkan驱动的连接。
        setupDebugMessenger();  // 初始化验证层调试回调，便于在调试阶段捕获错误信息。
        // surface须在instance创建后，physical device前创建，会影响
        createSurface();  // 为当前窗口创建Vulkan可呈现的表面对象。
        pickPhysicalDevice();  // 从所有可用GPU中选择一个满足条件的物理设备。
        createLogicalDevice();  // 根据物理设备创建逻辑设备，供后续资源和命令提交使用。
        createCommandPool();  // 创建命令池，后续可以从中分配命令缓冲区。
    }

    // 析构函数释放设备、命令池、窗口表面以及Vulkan实例资源，避免泄漏。
    CzxDevice::~CzxDevice() {  // 在对象销毁时释放所有由Vulkan创建的资源，避免泄漏。
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);  // 释放命令池，后续已经分配的命令缓冲区也会随之失效。
        vkDestroyDevice(m_device, nullptr);  // 销毁逻辑设备，释放其占用的驱动资源。

        if (enableValidationLayers) {  // 只有启用了验证层时才需要销毁调试消息器。
            DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);  // 释放调试消息器对象，避免回调残留。
        }

        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);  // 销毁当前窗口对应的Vulkan呈现表面。
        vkDestroyInstance(m_instance, nullptr);  // 释放Vulkan实例本身占用的底层资源。
    }

    // 创建Vulkan实例，初始化应用与Vulkan驱动之间的连接，并准备验证层和扩展配置。
    void CzxDevice::createInstance() {  // 这个函数负责初始化Vulkan实例，作为后续所有渲染资源创建的起点。
        if (enableValidationLayers && !checkValidationLayerSupport()) {  // 先检查所需验证层是否在当前平台上可用。
            throw std::runtime_error("validation layers requested, but not available!");  // 发现缺失时抛出异常，终止初始化流程。
        }

        VkApplicationInfo appInfo = {};  // 创建并清零应用信息结构体，避免残留字段影响Vulkan配置。
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;  // 告诉Vulkan该结构体的类型，便于正确解析其内容。
        appInfo.pApplicationName = "RealTimeRenderer App";  // 给应用起名字，便于调试器和驱动展示。
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);  // 设置应用版本号，方便识别和调试。
        appInfo.pEngineName = "No Engine";  // 设置引擎名，说明当前程序并没有使用完整引擎框架。
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);  // 设置引擎版本号，便于排查兼容性问题。
        appInfo.apiVersion = VK_API_VERSION_1_4;  // 指定当前项目期望使用的Vulkan API版本。

        VkInstanceCreateInfo createInfo = {};  // 创建实例创建信息结构体，描述Vulkan实例的配置参数。
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;  // 标记该结构体类型，告诉Vulkan该结构体的含义。
        createInfo.pApplicationInfo = &appInfo;  // 把应用信息挂进创建信息中，供实例创建时使用。

        auto extensions = getRequiredExtensions();  // 获取当前平台需要的实例扩展，包含GLFW和调试相关扩展。
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());  // 设置要启用的扩展数量。
        createInfo.ppEnabledExtensionNames = extensions.data();  // 把扩展名称数组传给Vulkan实例创建函数。

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;  // 预留调试消息器的创建信息结构体，只有启用验证层时才会使用。
        if (enableValidationLayers) {  // 如果启用了验证层，就把相关层和调试回调信息一起注入实例创建信息。
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());  // 设置启用的验证层数量。
            createInfo.ppEnabledLayerNames = validationLayers.data();  // 传入验证层名称数组，告诉Vulkan需要加载哪些层。

            populateDebugMessengerCreateInfo(debugCreateInfo);  // 填写调试消息器的回调配置，决定收集哪些消息。
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;  // 将调试消息器创建信息挂到实例创建链中。
        }
        else {  // 如果没有启用验证层，就关闭层和调试链的配置。
            createInfo.enabledLayerCount = 0;  // 设置验证层数量为0，表示不加载任何验证层。
            createInfo.pNext = nullptr;  // 清空扩展链，避免后续误使用调试信息结构体。
        }

        if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {  // 使用上面配置正式创建Vulkan实例。
            throw std::runtime_error("failed to create instance!");  // 创建失败时抛出异常，阻止后续初始化继续进行。
        }

        hasGflwRequiredInstanceExtensions();  // 检查并确认实例级扩展已满足后续窗口和调试相关功能的需求。
    }

    // 枚举所有支持Vulkan的物理设备，并选择一个满足条件的设备作为当前渲染目标。
    void CzxDevice::pickPhysicalDevice() {  // 这个函数负责从所有可用GPU中挑出一个适合当前项目的物理设备。
        uint32_t deviceCount = 0;  // 记录当前系统中可枚举到的物理设备数量。
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);  // 先获取设备数量，便于分配容器。
        if (deviceCount == 0) {  // 如果没有任何支持Vulkan的物理设备，说明当前环境无法运行渲染。
            throw std::runtime_error("failed to find GPUs with Vulkan support!");  // 抛出异常，明确提示环境问题。
        }
        std::cout << "Device count: " << deviceCount << std::endl;  // 输出设备数量，方便调试和确认枚举结果。
        std::vector<VkPhysicalDevice> devices(deviceCount);  // 根据枚举数量创建物理设备句柄数组。
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());  // 把实际枚举出的设备句柄写入数组。

        for (const auto& device : devices) {  // 依次检查每个设备是否满足当前项目要求。
            if (isDeviceSuitable(device)) {  // 如果设备满足队列族、扩展和交换链条件，就选为当前设备。
                m_physicalDevice = device;  // 保存该设备句柄，后续创建逻辑设备时使用。
                break;  // 找到合适设备后即可停止继续枚举。
            }
        }

        if (m_physicalDevice == VK_NULL_HANDLE) {  // 如果没有找到任何满足条件的设备，说明当前系统不符合要求。
            throw std::runtime_error("failed to find a suitable GPU!");  // 抛出异常，提醒用户更换或升级GPU环境。
        }

        vkGetPhysicalDeviceProperties(m_physicalDevice, &m_properties);  // 获取当前物理设备的属性，包含名称和类型等信息。
        std::cout << "physical device: " << m_properties.deviceName << std::endl;  // 输出设备名称，方便确认实际使用的是哪张显卡。
    }

    // 根据物理设备的队列族信息创建逻辑设备，并获取图形和呈现队列句柄。
    void CzxDevice::createLogicalDevice() {  // 这个函数负责在选中的物理设备上创建一个可提交命令的逻辑设备。
        QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);  // 查找图形和呈现队列所在的队列族索引。

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;  // 先准备一个队列创建信息列表，后续一次性创建所需队列。
        std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };  // 使用集合去重，确保图形和呈现队列只创建一次。

        float queuePriority = 1.0f;  // 设置队列优先级0-1，数值越大越优先被调度。
        for (uint32_t queueFamily : uniqueQueueFamilies) {  // 为每个唯一的队列族创建相应的队列配置。
            VkDeviceQueueCreateInfo queueCreateInfo = {};  // 创建并清零队列创建信息结构体。
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;  // 标记结构体类型，供Vulkan识别。
            queueCreateInfo.queueFamilyIndex = queueFamily;  // 指定当前要创建队列的队列族索引。
            queueCreateInfo.queueCount = 1;  // 每个队列族只创建一个队列，简化逻辑。
            queueCreateInfo.pQueuePriorities = &queuePriority;  // 设置队列优先级，影响调度顺序。
            queueCreateInfos.push_back(queueCreateInfo);  // 把配置加入列表，随后提交给逻辑设备创建函数。
        }

        VkPhysicalDeviceFeatures deviceFeatures = {};  // 创建设备特性结构体，声明当前逻辑设备所需的功能。
        deviceFeatures.samplerAnisotropy = VK_TRUE;  // 开启各向异性采样支持，后续纹理采样更真实。

        VkDeviceCreateInfo createInfo = {};  // 创建逻辑设备创建信息结构体。
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;  // 标记结构体类型。

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());  // 设置队列创建信息数量。
        createInfo.pQueueCreateInfos = queueCreateInfos.data();  // 把队列配置数组传给Vulkan。

        createInfo.pEnabledFeatures = &deviceFeatures;  // 指定当前逻辑设备需要启用的特性集合。
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());  // 设置设备扩展数量。
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();  // 把设备扩展名称数组传给Vulkan。

        // might not really be necessary anymore because device specific validation layers
        // have been deprecated
        if (enableValidationLayers) {  // 如果开启验证层，则同步启用对应的层配置。
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());  // 设置验证层数量。
            createInfo.ppEnabledLayerNames = validationLayers.data();  // 传递验证层名称数组。
        }
        else {  // 如果没有启用验证层，就关闭层配置。
            createInfo.enabledLayerCount = 0;  // 不加载任何验证层。
        }

        if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS) {  // 使用配置创建逻辑设备句柄。
            throw std::runtime_error("failed to create logical device!");  // 创建失败时抛出异常。
        }

        vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_graphicsQueue);  // 获取图形队列句柄，供后续提交绘制命令使用。
        vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_presentQueue);  // 获取呈现队列句柄，供后续提交交换链显示使用。
    }

    // 创建命令池，后续用于分配命令缓冲区执行一次性或循环提交的GPU命令。
    void CzxDevice::createCommandPool() {  // 这个函数负责为设备创建命令池，后续所有命令缓冲区都是从这里分配。
        QueueFamilyIndices queueFamilyIndices = findPhysicalQueueFamilies();  // 从成员函数中查找适合创建命令池的队列族。

        VkCommandPoolCreateInfo poolInfo = {};  // 创建命令池创建信息结构体。
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;  // 标记该结构体类型。
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();  // 指定命令池关联的图形队列族。
        poolInfo.flags =
            VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;  // 设置命令池的用途标志，方便短命令和重置命令缓冲区。

        if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {  // 根据配置真正创建命令池对象。
            throw std::runtime_error("failed to create command pool!");  // 创建失败时抛出异常。
        }
    }

    // 调用窗口对象为当前Vulkan实例创建窗口表面，以便后续交换链和呈现流程使用。
    void CzxDevice::createSurface() {  // 这个函数把当前窗口和Vulkan实例连接起来，形成可用于呈现的表面。
        m_window.createWindowSurface(m_instance, &m_surface);  // 通过窗口对象把表面句柄写入成员变量。
    }

    // 检查物理设备是否同时满足队列族、扩展和交换链能力要求。
    bool CzxDevice::isDeviceSuitable(VkPhysicalDevice device) {  // 这个函数用于判断某个物理设备是否适合当前渲染项目。
        QueueFamilyIndices indices = findQueueFamilies(device);  // 先查找该设备是否具备图形和呈现队列族。

        bool extensionsSupported = checkDeviceExtensionSupport(device);  // 检查该设备是否支持当前项目所需的设备扩展。

        bool swapChainAdequate = false;  // 默认先假设交换链能力不足，随后再根据实际情况更新。
        if (extensionsSupported) {  // 只有设备扩展都支持时才有必要继续检查交换链能力。
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);  // 查询设备对交换链格式和呈现模式的支持情况。
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();  // 只要存在可用格式和呈现模式，就认为交换链可用。
        }

        VkPhysicalDeviceFeatures supportedFeatures;  // 创建特性集合变量，用于读取设备支持的功能。
        vkGetPhysicalDeviceFeatures(device, &supportedFeatures);  // 获取当前设备支持的特性标志。

        return indices.isComplete() && extensionsSupported && swapChainAdequate &&
            supportedFeatures.samplerAnisotropy;  // 只有同时满足所有条件时，设备才被判定为适用。
    }

    // 配置调试消息器的回调参数，决定接收哪些严重级别和消息类型的输出。
    void CzxDevice::populateDebugMessengerCreateInfo(
        VkDebugUtilsMessengerCreateInfoEXT& createInfo) {  // 这个函数负责填写调试消息器回调相关参数。
        createInfo = {};  // 清零结构体，避免残留无效字段影响调试配置。
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;  // 标记结构体类型，告诉Vulkan它是调试消息器配置结构。
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;  // 只关注警告和错误等级的消息，避免过多调试输出。
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;  // 同时收集通用、验证和性能相关的功能性消息。
        createInfo.pfnUserCallback = debugCallback;  // 设置回调函数地址，Vulkan遇到消息时会调用它。
        createInfo.pUserData = nullptr;  // Optional  // 保留用户数据指针，当前未使用，置空即可。
    }

    // 初始化验证层调试消息器，便于在开发阶段定位Vulkan调用错误。
    void CzxDevice::setupDebugMessenger() {  // 这个函数负责创建调试消息器并挂载回调。
        if (!enableValidationLayers) return;  // 没有启用验证层时，不需要创建调试消息器。
        VkDebugUtilsMessengerCreateInfoEXT createInfo;  // 先准备调试消息器的创建信息结构体。
        populateDebugMessengerCreateInfo(createInfo);  // 填写消息器回调相关配置。
        if (CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS) {  // 用实例和配置信息创建调试消息器。
            throw std::runtime_error("failed to set up debug messenger!");  // 创建失败时抛出异常，提醒开发者检查验证层配置。
        }
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

    // 查找物理设备中同时支持图形和呈现的队列族，以便创建逻辑设备和提交命令。
    QueueFamilyIndices CzxDevice::findQueueFamilies(VkPhysicalDevice device) {  // 这个函数用于找到适合图形和呈现工作负载的队列族。
        QueueFamilyIndices indices;  // 创建结果结构体，保存找到的队列族索引。

        uint32_t queueFamilyCount = 0;  // 保存队列族数量。
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);  // 先枚举队列族数量。

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);  // 按数量创建队列族属性数组。
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());  // 把真实队列族属性写入数组。

        int i = 0;  // 用于记录当前遍历到的队列族序号。
        for (const auto& queueFamily : queueFamilies) {  // 遍历所有队列族，判断它们是否支持图形和呈现。
            if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {  // 如果队列族中存在可用队列且支持图形能力，就记录它。
                indices.graphicsFamily = i;  // 保存图形队列族索引。
            }
            VkBool32 presentSupport = false;  // 先默认假设当前队列族不支持呈现。
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);  // 查询当前队列族对当前窗口表面的呈现支持情况。
            if (queueFamily.queueCount > 0 && presentSupport) {  // 如果队列族存在并且支持呈现，就记录它。
                indices.presentFamily = i;  // 保存呈现队列族索引。
            }
            if (indices.isComplete()) {  // 如果图形和呈现队列族都已找到，就无需再继续枚举。
                break;  // 直接退出循环。
            }

            i++;  // 进入下一个队列族继续检查。
        }

        return indices;  // 返回找到的图形和呈现队列族索引。
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

    // 从候选格式中选择物理设备真正支持的格式，适用于深度缓冲等资源创建。
    VkFormat CzxDevice::findSupportedFormat(
        const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {  // 这个函数负责从一组候选格式中找出当前设备支持的格式。
        for (VkFormat format : candidates) {  // 依次检查每个候选格式是否满足要求。
            VkFormatProperties props;  // 创建格式属性结构体，用于保存设备对该格式的支持信息。
            vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);  // 查询当前设备对该格式的具体支持特性。

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {  // 如果是线性平铺并且满足所有所需特性，就返回该格式。
                return format;  // 直接返回可用格式。
            }
            else if (
                tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {  // 如果是最优平铺并且满足所需特性，就返回该格式。
                return format;  // 直接返回可用格式。
            }
        }
        throw std::runtime_error("failed to find supported format!");  // 如果所有候选格式都不满足，就抛出异常。
    }

    // 根据内存类型筛选条件和所需属性，找到适合当前资源的内存类型。
    uint32_t CzxDevice::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {  // 这个函数负责从物理设备内存类型中挑出合适的那一类。
        VkPhysicalDeviceMemoryProperties memProperties;  // 创建内存属性结构体，保存设备可用内存类型的信息。
        vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);  // 从物理设备获取当前内存池的详细属性。
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {  // 遍历所有内存类型，寻找最符合条件的那一个。
            if ((typeFilter & (1 << i)) &&
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {  // 同时满足位掩码和属性要求时，就认为是合适内存类型。
                return i;  // 返回对应的内存类型索引。
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");  // 没有找到合适内存类型时抛出异常。
    }

    // 创建一个Vulkan缓冲区并分配对应内存，供顶点、索引或上传数据使用。
    void CzxDevice::createBuffer(
        VkDeviceSize size,  // 需要创建的缓冲区大小，单位是字节。
        VkBufferUsageFlags usage,  // 指定缓冲区将要用于哪些Vulkan用途，比如顶点缓冲或上传缓冲。
        VkMemoryPropertyFlags properties,  // 指定缓冲区内存应具备的属性，例如HOST_VISIBLE或DEVICE_LOCAL。
        VkBuffer& buffer,  // 输出参数，接收创建出来的缓冲区句柄。
        VkDeviceMemory& bufferMemory) {  // 输出参数，接收分配好的显存或内存句柄。

        VkBufferCreateInfo bufferInfo{};  // 创建缓冲区创建信息结构体。
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;  // 告诉Vulkan该结构体的类型。
        bufferInfo.size = size;  // 设置缓冲区字节大小。
        bufferInfo.usage = usage;  // 设置缓冲区用途，决定后续能否绑定到顶点输入等流水线阶段。
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;  // 设置共享模式为独占，当前缓冲区只由一个队列族使用。

        if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {  // 调用Vulkan创建真正的缓冲区对象。
            throw std::runtime_error("failed to create vertex buffer!");  // 缓冲区创建失败时抛出异常。
        }

        VkMemoryRequirements memRequirements;  // 创建内存需求结构体，用于获取缓冲区所需的内存属性。
        vkGetBufferMemoryRequirements(m_device, buffer, &memRequirements);  // 查询当前缓冲区所需的内存大小和类型位掩码。

        VkMemoryAllocateInfo allocInfo{};  // 创建内存分配信息结构体。
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;  // 标记结构体类型。
        allocInfo.allocationSize = memRequirements.size;  // 设置分配大小，至少满足缓冲区需求。
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);  // 选择最合适的内存类型索引。

        if (vkAllocateMemory(m_device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {  // 为缓冲区分配实际的设备内存。
            throw std::runtime_error("failed to allocate vertex buffer memory!");  // 分配失败时抛出异常。
        }

        vkBindBufferMemory(m_device, buffer, bufferMemory, 0);  // 把刚刚分配的内存绑定到缓冲区上，形成可用的缓冲区资源。
    }

    // 开始一次性提交的命令缓冲区，用于执行临时性的GPU命令，如拷贝缓冲区。
    VkCommandBuffer CzxDevice::beginSingleTimeCommands() {  // 这个函数创建一个只提交一次的命令缓冲区，适合临时执行一次性GPU命令。
        VkCommandBufferAllocateInfo allocInfo{};  // 创建命令缓冲区分配信息结构体。
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;  // 标记结构体类型。
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;  // 说明这是主命令缓冲区，能直接提交给队列。
        allocInfo.commandPool = m_commandPool;  // 指定命令缓冲区来自哪个命令池。
        allocInfo.commandBufferCount = 1;  // 每次只分配一个命令缓冲区。

        VkCommandBuffer commandBuffer;  // 声明命令缓冲区句柄，接收分配结果。
        vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);  // 从命令池中分配一块命令缓冲区。

        VkCommandBufferBeginInfo beginInfo{};  // 创建命令缓冲区开始记录信息结构体。
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;  // 标记结构体类型。
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  // 告诉Vulkan这次命令缓冲区只提交一次，适合临时操作。

        vkBeginCommandBuffer(commandBuffer, &beginInfo);  // 开启命令缓冲区记录，后续命令会写入其中。
        return commandBuffer;  // 把可用命令缓冲区句柄返回给调用方。
    }

    // 结束一次性命令缓冲区并提交到图形队列执行，随后释放命令缓冲区资源。
    void CzxDevice::endSingleTimeCommands(VkCommandBuffer commandBuffer) {  // 这个函数负责结束一次性命令缓冲区的记录并真正提交执行。
        vkEndCommandBuffer(commandBuffer);  // 结束命令缓冲区的录制，后续可以提交。

        VkSubmitInfo submitInfo{};  // 创建提交信息结构体，用于描述要提交的命令缓冲区。
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;  // 标记结构体类型。
        submitInfo.commandBufferCount = 1;  // 这次提交只有一个命令缓冲区。
        submitInfo.pCommandBuffers = &commandBuffer;  // 指定要提交的命令缓冲区句柄。

        vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);  // 把命令缓冲区提交到图形队列执行。
        vkQueueWaitIdle(m_graphicsQueue);  // 等待图形队列空闲，确保一次性命令已经执行完成。

        vkFreeCommandBuffers(m_device, m_commandPool, 1, &commandBuffer);  // 执行完毕后把命令缓冲区释放回命令池。
    }

    // 用一次性命令把源缓冲区内容复制到目标缓冲区。
    void CzxDevice::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {  // 这个函数用于把一个缓冲区的内容复制到另一个缓冲区。
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();  // 先获得一个一次性命令缓冲区，方便临时执行拷贝命令。

        VkBufferCopy copyRegion{};  // 创建复制区域描述结构体，定义源和目标偏移与大小。
        copyRegion.srcOffset = 0;  // Optional  // 复制时源缓冲区偏移量为0，表示从起点开始复制。
        copyRegion.dstOffset = 0;  // Optional  // 复制时目标缓冲区偏移量为0，表示写入目标起点。
        copyRegion.size = size;  // 设置本次拷贝的字节数量。
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);  // 把命令记录进一次性命令缓冲区。

        endSingleTimeCommands(commandBuffer);  // 提交并等待执行，然后释放命令缓冲区。
    }

    // 把缓冲区内容拷贝到图像中，常用于将CPU上传的数据写入纹理或深度图像。
    void CzxDevice::copyBufferToImage(
        VkBuffer buffer,  // 源缓冲区句柄，包含要写入图像的数据。
        VkImage image,  // 目标图像句柄，接收缓冲区内容。
        uint32_t width,  // 目标图像的宽度，用于描述复制区域的尺寸。
        uint32_t height,  // 目标图像的高度，用于描述复制区域的尺寸。
        uint32_t layerCount) {  // 目标图像的层数，适用于数组纹理或立方体贴图。
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();  // 先获取一次性命令缓冲区，执行临时图像拷贝命令。

        VkBufferImageCopy region{};  // 创建缓冲区到图像拷贝的区域描述结构体。
        region.bufferOffset = 0;  // 从源缓冲区的起始位置开始复制。
        region.bufferRowLength = 0;  // 不进行行间距补齐，按连续排布处理。
        region.bufferImageHeight = 0;  // 不进行图像高度补齐，按连续排布处理。

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;  // 说明当前复制的是颜色通道内容。
        region.imageSubresource.mipLevel = 0;  // 指向mip 0级别的纹理内容。
        region.imageSubresource.baseArrayLayer = 0;  // 从第0层数组开始复制。
        region.imageSubresource.layerCount = layerCount;  // 指定复制的层数。

        region.imageOffset = { 0, 0, 0 };  // 目标图像复制起始偏移量设为左上角原点。
        region.imageExtent = { width, height, 1 };  // 设置目标图像的复制尺寸，深度为1。

        vkCmdCopyBufferToImage(
            commandBuffer,  // 传入当前正在记录的命令缓冲区。
            buffer,  // 源缓冲区句柄。
            image,  // 目标图像句柄。
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,  // 指定目标图像当前布局满足传输拷贝需求。
            1,  // 只有一个拷贝区域。
            &region);  // 传入该区域描述结构体。
        endSingleTimeCommands(commandBuffer);  // 提交并等待拷贝执行完成。
    }

    // 根据给定的图像创建信息创建图像资源并分配内存，供渲染目标或纹理使用。
    void CzxDevice::createImageWithInfo(
        const VkImageCreateInfo& imageInfo,  // 描述目标图像的格式、尺寸、用法和布局等信息。
        VkMemoryPropertyFlags properties,  // 指定图像内存应具备的属性，例如DEVICE_LOCAL。
        VkImage& image,  // 输出参数，接收创建出来的图像句柄。
        VkDeviceMemory& imageMemory) {  // 输出参数，接收分配好的图像内存句柄。
        if (vkCreateImage(m_device, &imageInfo, nullptr, &image) != VK_SUCCESS) {  // 根据图像创建信息真正创建图像对象。
            throw std::runtime_error("failed to create image!");  // 图像创建失败时抛出异常。
        }

        VkMemoryRequirements memRequirements;  // 创建内存需求结构体，用于查询图像所需的内存规格。
        vkGetImageMemoryRequirements(m_device, image, &memRequirements);  // 计算此图像需要的内存大小和类型位掩码。

        VkMemoryAllocateInfo allocInfo{};  // 创建内存分配信息结构体。
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;  // 标记结构体类型。
        allocInfo.allocationSize = memRequirements.size;  // 设置需要分配的内存大小。
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);  // 选择适合当前图像的内存类型索引。

        if (vkAllocateMemory(m_device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {  // 为图像分配实际的设备内存。
            throw std::runtime_error("failed to allocate image memory!");  // 分配失败时抛出异常。
        }

        if (vkBindImageMemory(m_device, image, imageMemory, 0) != VK_SUCCESS) {  // 把已分配内存绑定到图像对象上。
            throw std::runtime_error("failed to bind image memory!");  // 绑定失败时抛出异常。
        }
    }

    VkImageView CzxDevice::createImageView(
        VkImage image,
        VkFormat format,
        VkImageAspectFlags aspectMask,
        uint32_t mipLevels,
        uint32_t baseMipLevel,
        uint32_t arrayLayers,
        uint32_t baseArrayLayer,
        VkImageViewType viewType) {

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = viewType;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspectMask;
        viewInfo.subresourceRange.baseMipLevel = baseMipLevel;
        viewInfo.subresourceRange.levelCount = mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = baseArrayLayer;
        viewInfo.subresourceRange.layerCount = arrayLayers;

        VkImageView imageView;
        if (vkCreateImageView(m_device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image view!");
        }
        return imageView;
    }

    VkSampler CzxDevice::createDefaultSampler() {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

        VkSampler sampler;
        if (vkCreateSampler(m_device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create sampler!");
        }
        return sampler;
    }

    void CzxDevice::transitionImageLayout(
        VkImage image,
        VkFormat format,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        uint32_t mipLevels,
        uint32_t baseMipLevel,
        uint32_t arrayLayers,
        uint32_t baseArrayLayer,
        VkImageAspectFlags aspectMask) {

        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = aspectMask;
        barrier.subresourceRange.baseMipLevel = baseMipLevel;
        barrier.subresourceRange.levelCount = mipLevels;
        barrier.subresourceRange.baseArrayLayer = baseArrayLayer;
        barrier.subresourceRange.layerCount = arrayLayers;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        }
        else {
            throw std::runtime_error("Unsupported layout transition!");
        }

        vkCmdPipelineBarrier(
            commandBuffer,
            sourceStage, destinationStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        endSingleTimeCommands(commandBuffer);
    }

}  // namespace lve