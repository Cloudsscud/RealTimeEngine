#include <czx_imgui_renderer.h>

// ImGui 核心及后端
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

// std
#include <stdexcept>

namespace czx {
    static void CheckVKResult(VkResult err) {
        if (err == 0) {
            return;
        }
        fprintf(stderr, "[Vulkan] Error: VkResult = %d\n", err);

        if (err < 0) {
            abort();
        }
    }


    CzxImGuiRenderer::CzxImGuiRenderer(CzxDevice& device, CzxWindow& window,
        VkFormat colorFormat, VkFormat depthFormat,
        uint32_t imageCount)
        : m_device(device), m_window(window),
        m_colorFormat(colorFormat), m_depthFormat(depthFormat),
        m_imageCount(imageCount) {

        createDescriptorPool();
        initImGui();
    }

    CzxImGuiRenderer::~CzxImGuiRenderer() {
        cleanup();
    }

    void CzxImGuiRenderer::createDescriptorPool() {
        CzxDescriptorPool::Builder poolBuilder(m_device);
        poolBuilder.setMaxSets(1000)
            .addPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, 1000)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000)
            .addPoolSize(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000)
            .addPoolSize(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000)
            .setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
        m_descriptorPool = poolBuilder.build();
    }

    void CzxImGuiRenderer::initImGui() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext(); // 创建上下文

        ImGuiIO& io = ImGui::GetIO();   // 管理的系统对象
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;   // 启用键盘导航
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableSetMousePos;    // 启用鼠标位置/暂停功能

        auto extent = m_window.getExtent();
        io.DisplaySize = ImVec2((float)extent.width, (float)extent.height);
        
        ImGui::GetStyle().FontScaleMain = 1.5f; // 字体大小

        ImGui::StyleColorsDark();   // 深色风格

        // 初始化 GLFW 后端
        bool InstallGLFWCallbacks = true;   // 决定是否安装glfw回调:决定键盘和鼠标是否正常工作
        if (!ImGui_ImplGlfw_InitForVulkan(m_window.getGLFWwindow(), InstallGLFWCallbacks)) {
            throw std::runtime_error("Failed to initialize ImGui GLFW backend");
        }

        // 配置 Vulkan 后端（动态渲染模式）
        ImGui_ImplVulkan_InitInfo initInfo = {
            .ApiVersion = VK_API_VERSION_1_4,   // 与instance一致
            .Instance = m_device.getInstance(),
            .PhysicalDevice = m_device.physicalDevice().Selected().m_physDevice,
            .Device = m_device.device(),
            .QueueFamily = m_device.graphicsQueueFamily(),
            .Queue = m_device.graphicsQueue(),
            .DescriptorPool = m_descriptorPool->getDescriptorPool(),
            .DescriptorPoolSize = 0,
            .MinImageCount = m_imageCount,
            .ImageCount = m_imageCount,
            .PipelineCache = nullptr,
            .PipelineInfoMain = {
                .RenderPass = nullptr,
                .Subpass = 0,
                .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
                // .ExtraDynamicStates = ,
                .PipelineRenderingCreateInfo = {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
                    .pNext = nullptr,
                    .colorAttachmentCount = 1,
                    .pColorAttachmentFormats = &m_colorFormat,
                    .depthAttachmentFormat = m_depthFormat,
                    .stencilAttachmentFormat = VK_FORMAT_UNDEFINED
                },
                // .SwapChainImageUsage = 
            },
            // .PipelineInfoForViewports = ,
            .UseDynamicRendering = VK_TRUE,
            .Allocator = nullptr,
            .CheckVkResultFn = CheckVKResult
        };

        if (!ImGui_ImplVulkan_Init(&initInfo)) {
            throw std::runtime_error("Failed to initialize ImGui Vulkan backend");
        }

    }

    void CzxImGuiRenderer::cleanup() {
        m_device.freeCommandBuffers((uint32_t)m_commandBuffers.size(), m_commandBuffers.data());
        m_commandBuffers.clear();

        // 销毁ImGui资源
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

    }

    void CzxImGuiRenderer::render(VkCommandBuffer commandBuffer, const std::function<void()>& uiCallback) {
        if (!m_enabled) {
            return;
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // 2. 执行外部传入的 UI 回调（在这里放置你的 ImGui 窗口代码）
        if (uiCallback) {
            uiCallback();
        }

        // 3. 完成渲染并提交绘制数据
        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

    }


} // namespace czx