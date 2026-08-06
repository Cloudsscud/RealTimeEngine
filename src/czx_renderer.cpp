#include <czx_renderer.h>

//std
#include <stdexcept>
#include <array>

namespace czx {

	// 构造函数会初始化交换链与命令缓冲区，准备开始一帧一帧的渲染流程。
	CzxRenderer::CzxRenderer(CzxWindow& window, CzxDevice& device) :m_window(window), m_device(device)
	{
		recreateSwapChain();
		createCommandBuffers();
	}
	// 析构函数释放命令缓冲区资源，避免渲染器对象销毁后留下未释放的句柄。
	CzxRenderer::~CzxRenderer() {
		freeCommandBuffers();
	}

	// 在窗口大小变化或交换链失效时，重新创建交换链并同步渲染资源。
	void CzxRenderer::recreateSwapChain() {
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
			std::shared_ptr<CzxSwapChain> oldSwapChain = std::move(m_swapChain);
			m_swapChain = std::make_unique<CzxSwapChain>(m_device, extent, oldSwapChain);

			if (!oldSwapChain->compareSwapChainFormats(*m_swapChain.get())) {
				throw std::runtime_error("swap chain image/depth format has changed!");
				// 边界处理：可用回调函数通知app创建了不兼容渲染通道
			}

		}
	}


	// 为多帧渲染创建一组可复用的命令缓冲区。
	void CzxRenderer::createCommandBuffers() {
		m_commandBuffers.resize(CzxSwapChain::MAX_FRAMES_IN_FLIGHT);
		VkCommandBufferAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocateInfo.commandPool = m_device.getCommandPool();
		allocateInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

		if (vkAllocateCommandBuffers(m_device.device(), &allocateInfo, m_commandBuffers.data()) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate command buffers!");
		}
	}

	// 释放命令缓冲区资源，通常在渲染器销毁或重建时调用。
	void CzxRenderer::freeCommandBuffers() {
		vkFreeCommandBuffers(
			m_device.device(),
			m_device.getCommandPool(),
			static_cast<uint32_t>(m_commandBuffers.size()),
			m_commandBuffers.data());
		m_commandBuffers.clear();
	}

	// 开始一帧渲染，获取当前可用交换链图像并开始记录命令。
	VkCommandBuffer CzxRenderer::beginFrame() {
		assert(!m_isFrameStarted && "Cannot call beginFrame while already in progress");
 
		auto result = m_swapChain->acquireNextImage(&m_currentImageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			recreateSwapChain();
			return nullptr;	// 帧未能成功启动
		}

		if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		m_isFrameStarted = true;
		auto commandBuffer = getCurrentCommandBuffer();

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
			throw std::runtime_error("failed to begin record command buffer!");
		}

		return commandBuffer;
	}

	// 结束当前帧的命令记录，并提交渲染结果到交换链进行呈现。
	void CzxRenderer::endFrame() {
		assert(m_isFrameStarted && "Cannot call endFrame while frame not in progress");

		auto commandBuffer = getCurrentCommandBuffer();

		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
			throw std::runtime_error("failed to end record command buffer!");
		}

		auto result = m_swapChain->submitCommandBuffers(&commandBuffer, &m_currentImageIndex);
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_window.wasWindowResized()) {
			m_window.resetWindowResizedFlag();
			recreateSwapChain();
		}
		else if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to present swap chain image!");
		}

		m_isFrameStarted = false;
		m_currentFrameIndex = (m_currentFrameIndex+1)%CzxSwapChain::MAX_FRAMES_IN_FLIGHT;
	}

	// 开始当前交换链图像对应的渲染通道，并设置视口与裁剪区域。
	void CzxRenderer::beginSwapChainRenderPass(VkCommandBuffer commandbuffer) {
		assert(m_isFrameStarted && "Cannot call beginSwapChainRenderPass while frame not in progress");
		assert(commandbuffer == getCurrentCommandBuffer() && "Cannot begin render pass on command buffer from a different frame");

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = m_swapChain->getRenderPass();
		renderPassInfo.framebuffer = m_swapChain->getFrameBuffer(m_currentImageIndex);

		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = m_swapChain->getSwapChainExtent();

		std::array<VkClearValue, 2> clearValue{};
		clearValue[0].color = { 0.01f,0.01f,0.01f,0.1f };
		clearValue[1].depthStencil = { 1.0f, 0 };
		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValue.size());
		renderPassInfo.pClearValues = clearValue.data();

		vkCmdBeginRenderPass(commandbuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

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

		vkCmdSetViewport(commandbuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandbuffer, 0, 1, &scissor);
	}

	// 结束当前渲染通道，后续命令会进入下一个渲染阶段或结束。
	void CzxRenderer::endSwapChainRenderPass(VkCommandBuffer commandbuffer) {
		assert(m_isFrameStarted && "Cannot call endSwapChainRenderPass while frame not in progress");
		assert(commandbuffer == getCurrentCommandBuffer() && "Cannot end render pass on command buffer from a different frame");

		vkCmdEndRenderPass(commandbuffer);
	}

}	// namespace czx