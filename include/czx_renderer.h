#pragma once

#include <czx_window.h>
#include <czx_device.h>
#include <czx_swap_chain.h>

// std
#include <memory>
#include <vector>
#include <cassert>

namespace czx {
	// 负责管理一帧一帧的渲染流程，包括交换链、命令缓冲区、开始/结束渲染通道以及帧同步。
	class CzxRenderer {
	public:

		CzxRenderer(CzxWindow& window, CzxDevice& device);  // 根据窗口和设备创建渲染器并初始化交换链与命令缓冲区。
		~CzxRenderer();  // 释放渲染器拥有的命令缓冲区和交换链资源。

		CzxRenderer(const CzxRenderer&) = delete;  // 禁止拷贝，避免两个渲染器同时管理同一帧资源。
		CzxRenderer& operator=(const CzxRenderer&) = delete;  // 禁止赋值，防止重复释放命令缓冲区和交换链句柄。

		VkRenderPass getSwapChainRenderPass() const { return m_swapChain->getRenderPass(); }  // 返回当前交换链使用的渲染通道句柄。
		float getAspectRatio() const { return m_swapChain->extentAspectRatio(); }
		bool isFrameInProgress() const { return m_isFrameStarted; }  // 判断当前是否正在记录一帧命令。

		VkCommandBuffer getCurrentCommandBuffer() const {  // 获取当前帧正在使用的命令缓冲区。
			assert(m_isFrameStarted && "Cannot get command buffer when frame not in progress");
			return m_commandBuffers[m_currentFrameIndex];
		}
		
		int getFrameIndex() const {  // 获取当前帧索引，便于多缓冲逻辑定位当前帧。
			assert(m_isFrameStarted && "Cannot get frame index when frame not in progress");
			return m_currentFrameIndex;
		}

		VkCommandBuffer beginFrame();  // 开始一帧渲染，获取可用图像并开始记录命令。
		void endFrame();  // 结束一帧渲染，提交命令并呈现图像。
		void beginSwapChainRenderPass(VkCommandBuffer commandbuffer);  // 开始当前交换链对应的渲染通道。
		void endSwapChainRenderPass(VkCommandBuffer commandbuffer);  // 结束当前渲染通道。


	private:
		void createCommandBuffers();  // 创建一组命令缓冲区，供多帧渲染复用。
		void freeCommandBuffers();  // 释放命令缓冲区资源。
		void recreateSwapChain();  // 在窗口尺寸变化或交换链失效时重建交换链。

		CzxWindow& m_window;  // 对窗口对象的引用，用于获取窗口尺寸和重建状态。
		CzxDevice& m_device;  // 对设备对象的引用，提供逻辑设备和命令池访问能力。
		std::unique_ptr<CzxSwapChain> m_swapChain;  // 当前使用的交换链对象。
		std::vector<VkCommandBuffer> m_commandBuffers;  // 每帧对应的命令缓冲区集合。

		uint32_t m_currentImageIndex{};  // 当前帧正在渲染的交换链图像索引。
		int m_currentFrameIndex{};  // 当前使用的帧缓冲索引。
		bool m_isFrameStarted = false;  // 表示当前是否已经开始记录一帧命令。
	};
}	// namespace czx