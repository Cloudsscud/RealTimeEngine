#pragma once

#include <czx_device.h>

// libs
// 弧度制
#define GLM_FORCE_RADIANS
// 限定深度缓冲在[0,1]
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

// std
#include <vector>

namespace czx {

	// 负责把CPU端顶点数据上传到GPU，并提供绑定和绘制接口，作为渲染流程中的基础模型对象。
	class CzxModel {
	public:

		struct Vertex {
			glm::vec3 position;  // 顶点位置
			glm::vec3 color;  // 顶点颜色

			static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();  // 返回顶点输入绑定描述，告诉Vulkan顶点数据如何组织。
			static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();  // 返回顶点属性描述，告诉Vulkan位置和颜色分别对应哪些数据成员。
		};

		CzxModel(CzxDevice& device, const std::vector<Vertex>& vertices);  // 根据顶点数据在GPU上创建对应的顶点缓冲区。
		~CzxModel();

		CzxModel(const CzxModel&) = delete;
		CzxModel& operator=(const CzxModel&) = delete;

		void bind(VkCommandBuffer commandBuffer);  // 将顶点缓冲绑定到当前命令缓冲区，准备绘制。
		void draw(VkCommandBuffer commandBuffer);  // 发出一次绘制调用，使用当前绑定的顶点数据。

	private:
		void createVertexBuffer(const std::vector<Vertex>& vertices);  // 创建并填充顶点缓冲区。

		CzxDevice& m_device;  // 保存设备引用，便于访问逻辑设备和内存分配接口。
		VkBuffer m_vertexBuffer;  // 顶点缓冲句柄。
		VkDeviceMemory m_vertexBufferMemory;  // 顶点缓冲对应的内存对象。
		uint32_t m_vertexCount;  // 顶点数量，用于绘制调用时指定顶点个数。
	};
}	// namespace czx