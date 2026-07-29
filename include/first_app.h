#pragma once

#include <czx_window.h>
#include <czx_device.h>
#include <czx_pipeline.h>
#include <czx_swap_chain.h>
#include <czx_model.h>

// std
#include <memory>
#include <vector>

namespace czx {
	class FirstAPP {
	public:
		static constexpr int WIDTH = 800;
		static constexpr int HEIGHT = 600;

		FirstAPP();
		~FirstAPP();

		FirstAPP(const FirstAPP&) = delete;
		FirstAPP& operator=(const FirstAPP&) = delete;

		void run();
	private:
		void loadModels();
		void createPipelineLayout();
		void createPipeline();
		void createCommandBuffers();
		void drawFrame();

		CzxWindow m_window{WIDTH, HEIGHT, "Hello Vulkan!"};
		CzxDevice m_device{m_window};
		CzxSwapChain m_swapChain{ m_device, m_window.getExtent() };
		VkPipelineLayout m_pipelineLayout;
		std::unique_ptr<CzxPipeline> m_pipeline;
		std::vector<VkCommandBuffer> m_commandBuffers;
		std::unique_ptr<CzxModel> m_model;
	};
}	// namespace czx