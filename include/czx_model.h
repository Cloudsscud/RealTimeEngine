#pragma once

#include <czx_device.h>
#include <czx_buffer.h>
#include <czx_texture.h>

// libs
// 弧度制
#define GLM_FORCE_RADIANS
// 限定深度缓冲在[0,1]
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

// std
#include <vector>
#include <string>
#include <memory>

namespace czx {

	// 负责把CPU端顶点数据上传到GPU，并提供绑定和绘制接口，作为渲染流程中的基础模型对象。
	class CzxModel {
	public:

		struct Vertex {
			glm::vec3 position{};  // 顶点位置
			glm::vec3 color{};  // 顶点颜色
			glm::vec3 normal{};	// 法线
			glm::vec2 uv{};	// 纹理坐标

			static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();  // 返回顶点输入绑定描述，告诉Vulkan顶点数据如何组织。
			static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();  // 返回顶点属性描述，告诉Vulkan位置和颜色分别对应哪些数据成员。

			bool operator==(const Vertex& other)const {
				return position == other.position && color == other.color && normal == other.normal && uv == other.uv;
			}
		};

		// 辅助index和vertex均复制进buffer
		struct Builder {
			std::vector<Vertex> vertices{};	 // 顶点
			std::vector<uint32_t> indices{}; // 索引
			std::string m_textureFilePath = "";

			void loadModel(const std::string& filePath, const std::string& textureFilePath);
		};

		CzxModel(CzxDevice& device, const CzxModel::Builder& builder);  // 根据顶点和索引数据在GPU上创建对应的顶点、索引缓冲区。
		~CzxModel();

		CzxModel(const CzxModel&) = delete;
		CzxModel& operator=(const CzxModel&) = delete;

		// 从文件加载模型
		static std::unique_ptr<CzxModel> createModelFromFile(CzxDevice& device, const std::string& filePath, const std::string& textureFilePath = "");

		void setTexture(std::shared_ptr<CzxTexture> texture) { m_texture = texture; }
		std::shared_ptr<CzxTexture> getTexture() const { return m_texture; }
		bool hasTexture() const { return m_texture != nullptr; }

		void bind(VkCommandBuffer commandBuffer);  // 将顶点缓冲绑定到当前命令缓冲区，准备绘制。
		void draw(VkCommandBuffer commandBuffer);  // 发出一次绘制调用，使用当前绑定的顶点数据。

	private:
		void createVertexBuffer(const std::vector<Vertex>& vertices);  // 创建并填充顶点缓冲区。
		void createIndexBuffer(const std::vector<uint32_t>& indices);  // 创建并填充索引缓冲区。

		CzxDevice& m_device;  // 保存设备引用，便于访问逻辑设备和内存分配接口。
		std::unique_ptr<CzxBuffer> m_vertexBuffer;	// 抽象的顶点缓冲区
		uint32_t m_vertexCount;  // 顶点数量，用于绘制调用时指定顶点个数。

		bool m_hasIndexBuffer = false;	// 用于判断是否用到索引缓冲
		std::unique_ptr<CzxBuffer> m_indexBuffer;	// 抽象的索引缓冲区
		uint32_t m_indexCount;  // 顶点数量，用于绘制调用时指定索引个数。

		std::shared_ptr<CzxTexture> m_texture;	// 模型的纹理
	};
}	// namespace czx