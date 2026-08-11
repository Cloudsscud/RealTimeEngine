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
	// 推送常量数据结构，用来把每个对象的变换和颜色传递给着色器，最大128bytes=2*mat4x4
	struct SimplePushConstantData {
		glm::mat4 modelMatrix{ 1.f };
		glm::mat4 normalMatrix{ 1.f };
	};

	// 构造函数会先创建管线布局，再根据当前渲染通道创建具体的图形管线。
	SimpleRenderSystem::SimpleRenderSystem(CzxDevice& device, VkRenderPass renderPass,
		const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts)
		:m_device{device}
	{
		createPipelineLayout(descriptorSetLayouts);
		createPipeline(renderPass);
	}

	// 析构函数释放管线布局资源，避免内存泄漏。
	SimpleRenderSystem::~SimpleRenderSystem() {
		vkDestroyPipelineLayout(m_device.device(), m_pipelineLayout, nullptr);
	}


	// 为当前渲染系统创建管线布局，定义着色器可访问的推送常量范围。
	void SimpleRenderSystem::createPipelineLayout(const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) {
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(SimplePushConstantData);


		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
		pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
		if (vkCreatePipelineLayout(m_device.device(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("failed to create pipeline layout!");
		}
	}

	// 使用当前渲染通道创建图形管线，后续绘制会使用这条管线。
	void SimpleRenderSystem::createPipeline(VkRenderPass renderPass) {
		assert(m_pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

		PipelineConfigInfo pipelineConfig{};
		CzxPipeline::defaultPipelineConfigInfo(pipelineConfig);
		pipelineConfig.renderPass = renderPass;
		pipelineConfig.pipelineLayout = m_pipelineLayout;
		m_pipeline = std::make_unique<CzxPipeline>(m_device, "shaders/simple_shader.vert.spv", "shaders/simple_shader.frag.spv", pipelineConfig);
	}

	// 遍历所有游戏对象，更新其变换并依次绑定顶点缓冲和提交绘制调用。
	void SimpleRenderSystem::renderGameObjects(FrameInfo& frameInfo, std::vector<CzxGameObject>& gameObjects) {

		vkCmdBindDescriptorSets(
			frameInfo.commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			m_pipelineLayout,
			0,
			1,
			&frameInfo.globalDescriptorSet,
			0,
			nullptr);


		// render
		// bind vertex buffer后，drawCall之前，推送常量
		//多个不同常量图形便多次pushConstant 并 drawCall
		//将多个模型绑定放在一个缓冲区内可以减少大量的drawCall可以优化性能
		//但是此时不允许每个模型访问各自特有的推送常量
		m_pipeline->bind(frameInfo.commandBuffer);
		for (auto& obj : gameObjects) {

			SimplePushConstantData push{};
			push.modelMatrix = obj.m_transform.mat4();
			push.normalMatrix = obj.m_transform.normalMatrix();

			vkCmdPushConstants(
				frameInfo.commandBuffer,
				m_pipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(SimplePushConstantData), &push);

			// 绑定纹理
			vkCmdBindDescriptorSets(
				frameInfo.commandBuffer,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				m_pipelineLayout,
				0,
				1,
				&frameInfo.globalDescriptorSet,
				0,
				nullptr);

			obj.m_model->bind(frameInfo.commandBuffer);
			obj.m_model->draw(frameInfo.commandBuffer);
		}
	}


}	// namespace czx