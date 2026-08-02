#pragma once

#include <czx_device.h>
#include <czx_pipeline.h>
#include <czx_game_object.h>
#include <czx_camera.h>

// std
#include <memory>
#include <vector>

namespace czx {
	// 负责把游戏对象渲染到当前渲染通道中，封装管线绑定、推送常量和绘制调用流程。
	class SimpleRenderSystem {
	public:

		SimpleRenderSystem(CzxDevice& device, VkRenderPass renderPass);  // 根据设备和渲染通道创建渲染系统所需管线资源。
		~SimpleRenderSystem();  // 销毁管线布局和相关渲染资源。

		SimpleRenderSystem(const SimpleRenderSystem&) = delete;  // 禁止拷贝，避免多个渲染系统共享同一管线资源。
		SimpleRenderSystem& operator=(const SimpleRenderSystem&) = delete;  // 禁止赋值，防止重复释放资源。

		void renderGameObjects(VkCommandBuffer commandBuffer, std::vector<CzxGameObject>& gameObjects, const CzxCamera& camera);  // 遍历场景中的对象并执行绘制调用。
	private:
		void createPipelineLayout();  // 创建管线布局，定义着色器可访问的推送常量和资源绑定。
		void createPipeline(VkRenderPass renderPass);  // 使用当前渲染通道创建图形管线。

		CzxDevice& m_device;  // 对设备对象的引用，提供逻辑设备和资源创建能力。

		std::unique_ptr<CzxPipeline> m_pipeline;  // 当前使用的图形管线对象。
		VkPipelineLayout m_pipelineLayout;  // 管线布局句柄，控制着色器资源的绑定方式。
	};
}	// namespace czx