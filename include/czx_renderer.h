#pragma once

#include <czx_window.h>
#include <czx_device.h>
#include <czx_swap_chain.h>

// std
#include <memory>
#include <vector>
#include <cassert>

namespace czx {
	class CzxRenderer {
	public:

		CzxRenderer(CzxWindow& window, CzxDevice& device);
		~CzxRenderer();

		CzxRenderer(const CzxRenderer&) = delete;
		CzxRenderer& operator=(const CzxRenderer&) = delete;

		VkRenderPass getSwapChainRenderPass() const { return m_swapChain->getRenderPass(); }
		bool isFrameInProgress() const { return m_isFrameStarted; }

		VkCommandBuffer getCurrentCommandBuffer() const {
			assert(m_isFrameStarted && "Cannot get command buffer when frame not in progress");
			return m_commandBuffers[m_currentFrameIndex];
		}
		
		int getFrameIndex() const {
			assert(m_isFrameStarted && "Cannot get frame index when frame not in progress");
			return m_currentFrameIndex;
		}

		VkCommandBuffer beginFrame();
		void endFrame();
		void beginSwapChainRenderPass(VkCommandBuffer commandbuffer);
		void endSwapChainRenderPass(VkCommandBuffer commandbuffer);


	private:
		void createCommandBuffers();
		void freeCommandBuffers();
		void recreateSwapChain();

		CzxWindow& m_window;
		CzxDevice& m_device;
		std::unique_ptr<CzxSwapChain> m_swapChain;
		std::vector<VkCommandBuffer> m_commandBuffers;

		uint32_t m_currentImageIndex;
		int m_currentFrameIndex;
		bool m_isFrameStarted;
	};
}	// namespace czx