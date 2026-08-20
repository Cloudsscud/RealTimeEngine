#pragma once

#include <czx_utils.h>
#include <czx_device.h>
#include <czx_pipeline.h>
#include <czx_game_object.h>
#include <czx_camera.h>
#include <czx_frame_info.h>
#include <czx_descriptor.h>
#include <czx_buffer.h>
#include <czx_swap_chain.h>

// std
#include <memory>
#include <vector>
#include <unordered_map>

namespace czx {

	// 全局 UBO 数据结构，与着色器匹配
	struct GlobalUbo {
		alignas(16) glm::mat4 projectionView{ 1.f };
		alignas(16) glm::vec3 directionToLight = glm::normalize(glm::vec3(3.0, -3.0, 2.0));
	};

	class SimpleRenderSystem {
	public:
		struct Settings {
			glm::vec3 lightDirection = glm::normalize(glm::vec3(3.0f, -3.0f, 2.0f));	// 光照方向
			float rotationSpeed = 0.5f;	// 模型绕自身y轴旋转速度
			bool enableRotation = true;	// 是否启用旋转
		};
		Settings& getSettings() { return m_settings; }	// 暴露给imgui的可变参数


		SimpleRenderSystem(CzxDevice& device, VkFormat colorFormat, VkFormat depthFormat);
		~SimpleRenderSystem();

		SimpleRenderSystem(const SimpleRenderSystem&) = delete;  // 禁止拷贝
		SimpleRenderSystem& operator=(const SimpleRenderSystem&) = delete;  // 禁止赋值

		// 每帧渲染调用，传入帧信息和游戏对象列表
		void renderGameObjects(FrameInfo& frameInfo, std::vector<CzxGameObject>& gameObjects);
	private:
		// 描述符相关创建函数
		void createDescriptorSetLayouts();
		void createDescriptorPools();
		void allocateGlobalDescriptorSets();
		void allocateModelDescriptorSet(const CzxGameObject& obj);
		void updateUbo(int frameIndex, const CzxCamera& camera);

		// 管线创建
		void createPipelineLayout();  // 创建管线布局
		void createPipeline(VkFormat colorFormat, VkFormat depthFormat);

		CzxDevice& m_device;

		// 描述符布局
		std::unique_ptr<CzxDescriptorSetLayout> m_globalLayout;
		std::unique_ptr<CzxDescriptorSetLayout> m_modelLayout;

		// 描述符池
		std::unique_ptr<CzxDescriptorPool> m_globalPool;
		std::unique_ptr<CzxDescriptorPool> m_modelPool;

		// 全局描述符集（每飞行帧一个）
		std::vector<VkDescriptorSet> m_globalDescriptorSets;
		// UBO 缓冲（每飞行帧一个）
		std::vector<std::unique_ptr<CzxBuffer>> m_uboBuffers;

		// 模型描述符集映射（对象ID -> 描述符集）
		std::unordered_map<CzxGameObject::id_t, VkDescriptorSet> m_modelDescriptorSets;

		VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
		std::unique_ptr<CzxPipeline> m_pipeline;  // 当前使用的图形管线对象

		static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = CzxSwapChain::MAX_FRAMES_IN_FLIGHT;

		Settings m_settings{};
	};
}	// namespace czx