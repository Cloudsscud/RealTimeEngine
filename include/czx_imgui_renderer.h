#pragma once

#include <czx_utils.h>
#include <czx_device.h>
#include <czx_window.h>
#include <czx_descriptor.h>

// 前向声明 ImGui 上下文（不需要暴露）
struct ImGuiContext;

namespace czx {

    class CzxImGuiRenderer {
    public:
        CzxImGuiRenderer(CzxDevice& device, CzxWindow& window,
            VkFormat colorFormat, VkFormat depthFormat,
            uint32_t imageCount);
        ~CzxImGuiRenderer();

        CzxImGuiRenderer(const CzxImGuiRenderer&) = delete;
        CzxImGuiRenderer& operator=(const CzxImGuiRenderer&) = delete;

        void setEnabled(bool enabled) { m_enabled = enabled; }
        bool isEnabled() const { return m_enabled; }

        /**
         * @brief 在主渲染通道内绘制 ImGui
         * @param commandBuffer 当前帧的主命令缓冲（已处于 beginRendering 和 endRendering 之间）
         * @param uiCallback 用于构建 ImGui 窗口的回调（例如放置 ImGui::Begin/End）
         */
        void render(VkCommandBuffer commandBuffer, const std::function<void()>& uiCallback = {});
    private:
        void initImGui();
        void createDescriptorPool();
        void cleanup();

        CzxWindow& m_window;
        CzxDevice& m_device;

        VkFormat m_colorFormat;
        VkFormat m_depthFormat;
        uint32_t m_imageCount;

        std::unique_ptr<CzxDescriptorPool> m_descriptorPool;
        std::vector<VkCommandBuffer> m_commandBuffers;

        bool m_enabled = true;
    };

} // namespace czx