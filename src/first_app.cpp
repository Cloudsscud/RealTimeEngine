#include <first_app.h>

//std
#include <stdexcept>
#include <array>

namespace czx {
	FirstAPP::FirstAPP() {
		loadModels();
		createPipelineLayout();
		createPipeline();
		createCommandBuffers();
	}
	FirstAPP::~FirstAPP() {
		vkDestroyPipelineLayout(m_device.device(), m_pipelineLayout, nullptr);
	}

	void FirstAPP::run() {
		while (!m_window.shouldClose()) {
			glfwPollEvents();	// 检查处理所有窗口事件
			drawFrame();
		}
		vkDeviceWaitIdle(m_device.device());
	}

	void FirstAPP::loadModels() {
		// {vector{Vertex{glm::vec2}}}
		std::vector<CzxModel::Vertex> vertices{
			{{0.0f,-0.5f}},
			{{0.5f,0.5f}},
			{{-0.5f, 0.5f}}
		};

		m_model = std::make_unique<CzxModel>(m_device, vertices);
	}


	void FirstAPP::createPipelineLayout() {
		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 0;
		pipelineLayoutInfo.pSetLayouts = nullptr;
		pipelineLayoutInfo.pushConstantRangeCount = 0;
		pipelineLayoutInfo.pPushConstantRanges = nullptr;
		if (vkCreatePipelineLayout(m_device.device(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("failed to create pipeline layout!");
		}
	}

	void FirstAPP::createPipeline() {
		auto pipelineConfig = CzxPipeline::defaultPipelineConfigInfo(m_swapChain.width(), m_swapChain.height());
		pipelineConfig.renderPass = m_swapChain.getRenderPass();
		pipelineConfig.pipelineLayout = m_pipelineLayout;
		m_pipeline = std::make_unique<CzxPipeline>(m_device, "shaders/simple_shader.vert.spv", "shaders/simple_shader.frag.spv", pipelineConfig);
	}

	void FirstAPP::createCommandBuffers() {
		m_commandBuffers.resize(m_swapChain.imageCount());
		VkCommandBufferAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocateInfo.commandPool = m_device.getCommandPool();
		allocateInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

		if (vkAllocateCommandBuffers(m_device.device(), &allocateInfo, m_commandBuffers.data()) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate command buffers!");
		}

		for (int i = 0; i < m_commandBuffers.size(); ++i) {
			VkCommandBufferBeginInfo beginInfo{};
			if (vkBeginCommandBuffer(m_commandBuffers[i], &beginInfo) != VK_SUCCESS) {
				throw std::runtime_error("failed to begin record command buffer!");
			}

			VkRenderPassBeginInfo renderPassInfo{};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = m_swapChain.getRenderPass();
			renderPassInfo.framebuffer = m_swapChain.getFrameBuffer(i);

			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = m_swapChain.getSwapChainExtent();

			std::array<VkClearValue, 2> clearValue{};
			clearValue[0].color = { 0.1f,0.1f,0.1f,0.1f };
			clearValue[1].depthStencil = { 1.0f, 0 };
			renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValue.size());
			renderPassInfo.pClearValues = clearValue.data();

			vkCmdBeginRenderPass(m_commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			m_pipeline->bind(m_commandBuffers[i]);
			m_model->bind(m_commandBuffers[i]);	// 绑定并绘制vertex buffer数据
			m_model->draw(m_commandBuffers[i]);

			vkCmdEndRenderPass(m_commandBuffers[i]);
			if (vkEndCommandBuffer(m_commandBuffers[i]) != VK_SUCCESS) {
				throw std::runtime_error("failed to end record command buffer!");
			}
		}
	}

	void FirstAPP::drawFrame() {
		uint32_t imageIndex;
		auto result = m_swapChain.acquireNextImage(&imageIndex);
		if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		result = m_swapChain.submitCommandBuffers(&m_commandBuffers[imageIndex], &imageIndex);
		if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to present swap chain image!");
		}
	}
}	// namespace czx