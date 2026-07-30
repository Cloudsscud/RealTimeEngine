#include <first_app.h>

//std
#include <stdexcept>
#include <array>

namespace czx {
	FirstAPP::FirstAPP() {
		loadModels();
		createPipelineLayout();
		recreateSwapChain();
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
			{{0.0f,-0.5f}, {1.0f, 0.0f, 0.0f}},
			{{0.5f,0.5f}, { 0.0f, 1.0f, 0.0f }},
			{{-0.5f, 0.5f},{0.0f, 0.0f, 1.0f}}
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
		assert(m_swapChain != nullptr && "Cannot create pipeline before swap chain");
		assert(m_pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

		PipelineConfigInfo pipelineConfig{};
		CzxPipeline::defaultPipelineConfigInfo(pipelineConfig);
		pipelineConfig.renderPass = m_swapChain->getRenderPass();
		pipelineConfig.pipelineLayout = m_pipelineLayout;
		m_pipeline = std::make_unique<CzxPipeline>(m_device, "shaders/simple_shader.vert.spv", "shaders/simple_shader.frag.spv", pipelineConfig);
	}

	void FirstAPP::recreateSwapChain() {
		auto extent = m_window.getExtent();
		while (extent.width == 0 || extent.height == 0) {
			extent = m_window.getExtent();
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(m_device.device());

		if (m_swapChain == nullptr) {
			m_swapChain = std::make_unique<CzxSwapChain>(m_device, extent);
		}
		else {
			m_swapChain = std::make_unique<CzxSwapChain>(m_device, extent, std::move(m_swapChain));
			if (m_swapChain->imageCount() != m_commandBuffers.size()) {
				freeCommandBuffers();
				createCommandBuffers();
			}
		}
		createPipeline();

	}


	void FirstAPP::createCommandBuffers() {
		m_commandBuffers.resize(m_swapChain->imageCount());
		VkCommandBufferAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocateInfo.commandPool = m_device.getCommandPool();
		allocateInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

		if (vkAllocateCommandBuffers(m_device.device(), &allocateInfo, m_commandBuffers.data()) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate command buffers!");
		}
	}

	void FirstAPP::freeCommandBuffers() {
		vkFreeCommandBuffers(
			m_device.device(),
			m_device.getCommandPool(),
			static_cast<uint32_t>(m_commandBuffers.size()),
			m_commandBuffers.data());
		m_commandBuffers.clear();
	}


	void FirstAPP::recordCommandBuffer(int imageIndex) {
		VkCommandBufferBeginInfo beginInfo{};
		if (vkBeginCommandBuffer(m_commandBuffers[imageIndex], &beginInfo) != VK_SUCCESS) {
			throw std::runtime_error("failed to begin record command buffer!");
		}

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = m_swapChain->getRenderPass();
		renderPassInfo.framebuffer = m_swapChain->getFrameBuffer(imageIndex);

		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = m_swapChain->getSwapChainExtent();

		std::array<VkClearValue, 2> clearValue{};
		clearValue[0].color = { 0.1f,0.1f,0.1f,0.1f };
		clearValue[1].depthStencil = { 1.0f, 0 };
		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValue.size());
		renderPassInfo.pClearValues = clearValue.data();

		vkCmdBeginRenderPass(m_commandBuffers[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		// 注意：视口与裁剪固定在管线中，不支持窗口Resize
		//配置视口，将NDC下的坐标映射到帧缓冲的像素区域
		VkViewport viewport{};
		viewport.x = 0.0f;	// 视口左上角在帧缓冲的坐标为0,0
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(m_swapChain->getSwapChainExtent().width);	// 视口宽高
		viewport.height = static_cast<float>(m_swapChain->getSwapChainExtent().height);
		viewport.minDepth = 0.0f;	// NDC的z[-1,1]映射到帧缓冲的深度[0,1]
		viewport.maxDepth = 1.0f;
		// 屏幕上的x = viewport.x+(NDC.x+1)/2*width
		// 屏幕上的y = viewport.y+(1-NDC.y)/2*width	// 左上角原点，y轴向下

		// 配置裁剪矩形，限定渲染矩形区域内的像素
		VkRect2D scissor{};
		scissor.offset = { 0, 0 };			// 裁剪矩形的左上角坐标int32_t
		scissor.extent = { m_swapChain->getSwapChainExtent() };	// uint32_t渲染整个窗口的像素->不裁剪

		vkCmdSetViewport(m_commandBuffers[imageIndex], 0, 1, &viewport);
		vkCmdSetScissor(m_commandBuffers[imageIndex], 0, 1, &scissor);

		m_pipeline->bind(m_commandBuffers[imageIndex]);
		m_model->bind(m_commandBuffers[imageIndex]);	// 绑定并绘制vertex buffer数据
		m_model->draw(m_commandBuffers[imageIndex]);

		vkCmdEndRenderPass(m_commandBuffers[imageIndex]);
		if (vkEndCommandBuffer(m_commandBuffers[imageIndex]) != VK_SUCCESS) {
			throw std::runtime_error("failed to end record command buffer!");
		}
	}


	void FirstAPP::drawFrame() {
		uint32_t imageIndex;
		auto result = m_swapChain->acquireNextImage(&imageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			recreateSwapChain();
			return;
		}

		if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		recordCommandBuffer(imageIndex);
		result = m_swapChain->submitCommandBuffers(&m_commandBuffers[imageIndex], &imageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_window.wasWindowResized()) {
			m_window.resetWindowResizedFlag();
			recreateSwapChain();
			return;
		}

		if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to present swap chain image!");
		}
	}
}	// namespace czx