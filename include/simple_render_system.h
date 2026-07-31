#pragma once

#include <czx_device.h>
#include <czx_pipeline.h>
#include <czx_game_object.h>

// std
#include <memory>
#include <vector>

namespace czx {
	class SimpleRenderSystem {
	public:

		SimpleRenderSystem(CzxDevice& device, VkRenderPass renderPass);
		~SimpleRenderSystem();

		SimpleRenderSystem(const SimpleRenderSystem&) = delete;
		SimpleRenderSystem& operator=(const SimpleRenderSystem&) = delete;

		void renderGameObjects(VkCommandBuffer commandBuffer, std::vector<CzxGameObject>& gameObjects);
	private:
		void createPipelineLayout();
		void createPipeline(VkRenderPass renderPass);

		CzxDevice& m_device;

		std::unique_ptr<CzxPipeline> m_pipeline;
		VkPipelineLayout m_pipelineLayout;
	};
}	// namespace czx