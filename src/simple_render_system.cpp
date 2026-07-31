#include <simple_render_system.h>

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
		glm::mat2 transform{ 1.f };	// 默认单位矩阵
		glm::vec2 offset;
		alignas(16) glm::vec3 color;	// 推送常量布局要对齐，vec
	};

	SimpleRenderSystem::SimpleRenderSystem(CzxDevice& device, VkRenderPass renderPass) :m_device{device}
	{
		createPipelineLayout();
		createPipeline(renderPass);
	}

	SimpleRenderSystem::~SimpleRenderSystem() {
		vkDestroyPipelineLayout(m_device.device(), m_pipelineLayout, nullptr);
	}


	void SimpleRenderSystem::createPipelineLayout() {
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

	void SimpleRenderSystem::createPipeline(VkRenderPass renderPass) {
		assert(m_pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

		PipelineConfigInfo pipelineConfig{};
		CzxPipeline::defaultPipelineConfigInfo(pipelineConfig);
		pipelineConfig.renderPass = renderPass;
		pipelineConfig.pipelineLayout = m_pipelineLayout;
		m_pipeline = std::make_unique<CzxPipeline>(m_device, "shaders/simple_shader.vert.spv", "shaders/simple_shader.frag.spv", pipelineConfig);
	}

	void SimpleRenderSystem::renderGameObjects(VkCommandBuffer commandBuffer, std::vector<CzxGameObject>& gameObjects) {
		// update
		int i = 0;
		for (auto& obj : gameObjects) {
			i += 1;
			obj.m_transform2d.rotation = glm::mod(obj.m_transform2d.rotation + 0.001f * i, glm::two_pi<float>());
		}

		// render
		// bind vertex buffer后，drawCall之前，推送常量
		//多个不同常量图形便多次pushConstant 并 drawCall
		//将多个模型绑定放在一个缓冲区内可以减少大量的drawCall可以优化性能
		//但是此时不允许每个模型访问各自特有的推送常量
		m_pipeline->bind(commandBuffer);
		for (auto& obj : gameObjects) {

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


}	// namespace czx