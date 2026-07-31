#include <first_app.h>

// libs
// 弧度制
#define GLM_FORCE_RADIANS
// 限定深度缓冲在[0,1]
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

//std
#include <stdexcept>
#include <array>

namespace czx {
	struct SimplePushConstantData {
		glm::mat2 transform{1.f};	// 默认单位矩阵
		glm::vec2 offset;
		alignas(16) glm::vec3 color;	// 推送常量布局要对齐，vec
	};

	FirstAPP::FirstAPP() {
		loadGameObjects();
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

	void FirstAPP::loadGameObjects() {
		// {vector{Vertex{glm::vec2}}}
		std::vector<CzxModel::Vertex> vertices{
			{{0.0f,-0.5f}, {1.0f, 0.0f, 0.0f}},
			{{0.5f,0.5f}, { 0.0f, 1.0f, 0.0f }},
			{{-0.5f, 0.5f},{0.0f, 0.0f, 1.0f}}
		};

		auto m_model = std::make_shared<CzxModel>(m_device, vertices);

		for (int i = 0; i < 10; ++i) {
			auto triangle = CzxGameObject::createGameObject();
			triangle.m_model = m_model;
			triangle.m_color = { .1f * i, .8f, 0.1f + 0.01 * i };
			triangle.m_transform2d.transform.x = .2f;
			triangle.m_transform2d.scale = { 1.5f, 0.5f };
			triangle.m_transform2d.rotation = .25f* glm::two_pi<float>();

			m_gameObjects.push_back(std::move(triangle));
		}

	}


	void FirstAPP::createPipelineLayout() {

		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(SimplePushConstantData);

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 0;
		pipelineLayoutInfo.pSetLayouts = nullptr;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
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
		clearValue[0].color = { 0.01f,0.01f,0.01f,0.1f };
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

		// 绘制调用接口
		renderGameObjects(m_commandBuffers[imageIndex]);

		vkCmdEndRenderPass(m_commandBuffers[imageIndex]);
		if (vkEndCommandBuffer(m_commandBuffers[imageIndex]) != VK_SUCCESS) {
			throw std::runtime_error("failed to end record command buffer!");
		}
	}

	void FirstAPP::renderGameObjects(VkCommandBuffer commandBuffer) {
		// update
		int i = 0;
		for (auto& obj : m_gameObjects) {
			i += 1;
			obj.m_transform2d.rotation = glm::mod(obj.m_transform2d.rotation + 0.001f * i, glm::two_pi<float>());
		}

		// render
		// bind vertex buffer后，drawCall之前，推送常量
		//多个不同常量图形便多次pushConstant 并 drawCall
		//将多个模型绑定放在一个缓冲区内可以减少大量的drawCall可以优化性能
		//但是此时不允许每个模型访问各自特有的推送常量
		m_pipeline->bind(commandBuffer);
		for (auto& obj : m_gameObjects) {

			SimplePushConstantData push{};
			push.offset = obj.m_transform2d.transform;
			push.color = obj.m_color;
			push.transform = obj.m_transform2d.mat2();

			vkCmdPushConstants(
				commandBuffer,
				m_pipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(SimplePushConstantData), &push);
			obj.m_model->bind(commandBuffer);
			obj.m_model->draw(commandBuffer);
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