#include <czx_model.h>
#include <czx_utils.h>

// libs
#define RAPIDOBJ_IMPLEMENTATION
#include <rapidobj/rapidobj.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

// std
#include <cassert>
#include <cstring>
#include <iostream>
#include <unordered_map>

namespace std {
	template<>
	struct hash<czx::CzxModel::Vertex> {
		size_t operator()(czx::CzxModel::Vertex const& vertex) const {
			size_t seed = 0;
			czx::hashCombine(seed, vertex.position, vertex.color, vertex.normal, vertex.uv);
			return seed;
		}
	};
}

namespace czx {
	// 构造函数会把顶点数据上传到GPU，并为后续绘制准备好顶点缓冲区。
	CzxModel::CzxModel(CzxDevice& device, const CzxModel::Builder& builder)
		:m_device(device)
	{
		createVertexBuffer(builder.vertices);
		createIndexBuffer(builder.indices);
	}

	// 析构函数释放模型对应的vertex buffer、index buffer和相关显存资源
	CzxModel::~CzxModel() {
		// 清理vertex buffer
		vkDestroyBuffer(m_device.device(), m_vertexBuffer, nullptr);
		vkFreeMemory(m_device.device(), m_vertexBufferMemory, nullptr);

		// 清理index buffer
		if (m_hasIndexBuffer) {
			vkDestroyBuffer(m_device.device(), m_indexBuffer, nullptr);
			vkFreeMemory(m_device.device(), m_indexBufferMemory, nullptr);
		}
	}

	std::unique_ptr<CzxModel> CzxModel::createModelFromFile(CzxDevice& device, const std::string& filePath) {
		Builder builder{};
		builder.loadModel(filePath);
		return std::make_unique<CzxModel>(device, builder);
	}


	//// 对于频繁更新数据可使用Host与GPU连接的缓冲区：创建并填充顶点缓冲区，把CPU端的顶点数据上传到GPU内存中。
	//void CzxModel::createVertexBuffer(const std::vector<Vertex>& vertices) {
	//	m_vertexCount = static_cast<uint32_t>(vertices.size());
	//	assert(m_vertexCount >= 3 && "Vertex count must be at least 3!");
	//	VkDeviceSize bufferSize = sizeof(vertices[0]) * m_vertexCount;
	//	m_device.createBuffer(
	//		bufferSize,
	//		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
	//		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	//		m_vertexBuffer,
	//		m_vertexBufferMemory);

	//	void* data;
	//	vkMapMemory(m_device.device(), m_vertexBufferMemory, 0, bufferSize, 0, &data);
	//	memcpy(data, vertices.data(), static_cast<uint32_t>(bufferSize));
	//	vkUnmapMemory(m_device.device(), m_vertexBufferMemory);
	//}
	//
	////  Host与GPU连接的缓冲区：创建并填充索引缓冲区，把CPU端的索引数据上传到GPU内存中。
	//void CzxModel::createIndexBuffer(const std::vector<uint32_t>& indices) {
	//	m_indexCount = static_cast<uint32_t>(indices.size());
	//	m_hasIndexBuffer = m_indexCount > 0;
	//	if (!m_hasIndexBuffer) {
	//		return;
	//	}

	//	// 计算这部分index buffer需要的字节数
	//	VkDeviceSize bufferSize = sizeof(indices[0]) * m_indexCount;
	//	// 在GPU设备上创建该大小的index buffer，并记录这部分buffer的句柄与内存句柄
	//	m_device.createBuffer(
	//		bufferSize,
	//		VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
	//		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	//		m_indexBuffer,
	//		m_indexBufferMemory);

	//	void* data;
	//	vkMapMemory(m_device.device(), m_vertexBufferMemory, 0, bufferSize, 0, &data);	// 将主机内存data(CPU)映射到这部分GPU内存的位置
	//	memcpy(data, indices.data(), static_cast<uint32_t>(bufferSize));	// 索引数据写入主机data时，同步到GPU的index buffer
	//	vkUnmapMemory(m_device.device(), m_indexBufferMemory);	// 数据不再变化，结束映射，自动回收主机的数据
	//}

	//  静态数据  效率最优的设备本地内存的缓冲区，不与host连接，但需要从与host连接的缓冲区拷贝：创建并填充索引缓冲区，把CPU端的索引数据上传到GPU内存中。
	void CzxModel::createIndexBuffer(const std::vector<uint32_t>& indices) {
		m_indexCount = static_cast<uint32_t>(indices.size());
		m_hasIndexBuffer = m_indexCount > 0;
		if (!m_hasIndexBuffer) {
			return;
		}

		// 计算这部分index buffer需要的字节数
		VkDeviceSize bufferSize = sizeof(indices[0]) * m_indexCount;

		// 创建暂存缓冲区,用于拷贝
		VkBuffer stageBuffer;
		VkDeviceMemory stageBufferMemory;
		m_device.createBuffer(
			bufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,	// 内存传输源
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,	// 暂存缓冲区确保host可见，且缓存一致性确保host主机更新时，将数据刷新到设备端
			stageBuffer,
			stageBufferMemory);

		void* data;
		vkMapMemory(m_device.device(), stageBufferMemory, 0, bufferSize, 0, &data);	// 将host数据(CPU)映射到设备端暂存缓冲区内存的位置
		memcpy(data, indices.data(), static_cast<uint32_t>(bufferSize));	// 数据写入host主机内存data时，vulkan自动同步到设备端GPU的暂存缓冲区
		vkUnmapMemory(m_device.device(), stageBufferMemory);	// 后续数据不再变化，结束映射，自动回收host的数据

		// 创建设备本地的index buffer，并记录这部分buffer的句柄与内存句柄
		m_device.createBuffer(
			bufferSize,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,	// 顶点缓冲且为缓冲区内存接收源
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,	// 设备本地缓冲区
			m_indexBuffer,
			m_indexBufferMemory);

		m_device.copyBuffer(stageBuffer, m_indexBuffer, bufferSize);

		// 清理暂存缓冲区
		vkDestroyBuffer(m_device.device(), stageBuffer, nullptr);
		vkFreeMemory(m_device.device(), stageBufferMemory, nullptr);
	}

	void CzxModel::createVertexBuffer(const std::vector<Vertex>& vertices) {

		m_vertexCount = static_cast<uint32_t>(vertices.size());
		assert(m_vertexCount >= 3 && "Vertex count must be at least 3!");

		// 计算这部分vertex buffer需要的字节数
		VkDeviceSize bufferSize = sizeof(vertices[0]) * m_vertexCount;

		// 创建暂存缓冲区,用于拷贝
		VkBuffer stageBuffer;
		VkDeviceMemory stageBufferMemory;
		m_device.createBuffer(
			bufferSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,	// 内存传输源
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,	// 暂存缓冲区确保host可见，且缓存一致性确保host主机更新时，将数据刷新到设备端
			stageBuffer,
			stageBufferMemory);

		void* data;
		vkMapMemory(m_device.device(), stageBufferMemory, 0, bufferSize, 0, &data);	// 将host数据(CPU)映射到设备端暂存缓冲区内存的位置
		memcpy(data, vertices.data(), static_cast<uint32_t>(bufferSize));	// 数据写入host主机内存data时，vulkan自动同步到设备端GPU的暂存缓冲区
		vkUnmapMemory(m_device.device(), stageBufferMemory);	// 后续数据不再变化，结束映射，自动回收host的数据

		// 创建设备本地的vertex buffer，并记录这部分buffer的句柄与内存句柄
		m_device.createBuffer(
			bufferSize,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,	// 顶点缓冲且为缓冲区内存接收源
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,	// 设备本地缓冲区
			m_vertexBuffer,
			m_vertexBufferMemory);

		m_device.copyBuffer(stageBuffer, m_vertexBuffer, bufferSize);

		// 清理暂存缓冲区
		vkDestroyBuffer(m_device.device(), stageBuffer, nullptr);
		vkFreeMemory(m_device.device(), stageBufferMemory, nullptr);
	}

	// 把顶点缓冲绑定到当前命令缓冲区，准备在后续绘制调用中使用。
	void CzxModel::bind(VkCommandBuffer commandBuffer) {
		VkBuffer buffers[] = { m_vertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
		if (m_hasIndexBuffer) {
			vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		}
	}


	// 发出一次绘制调用，绘制当前绑定的顶点缓冲内容。
	void CzxModel::draw(VkCommandBuffer commandBuffer) {
		if (m_hasIndexBuffer) {
			vkCmdDrawIndexed(commandBuffer, m_indexCount, 1, 0, 0, 0);
		}
		else {
			vkCmdDraw(commandBuffer, m_vertexCount, 1, 0, 0);
		}
	}

	// 描述顶点缓冲中的顶点数据如何以连续绑定方式进行输入。
	std::vector<VkVertexInputBindingDescription> CzxModel::Vertex::getBindingDescriptions() {
		std::vector< VkVertexInputBindingDescription> bindingDescriptions(1);
		bindingDescriptions[0].binding = 0;
		bindingDescriptions[0].stride = sizeof(Vertex);
		bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		return bindingDescriptions;
	}

	// 描述顶点属性在缓冲区中的布局，告诉Vulkan位置和颜色分别来自哪里。
	std::vector<VkVertexInputAttributeDescription> CzxModel::Vertex::getAttributeDescriptions() {
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions(2);
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0; // 对应vertex shader对应的position
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;	// vec3
		attributeDescriptions[0].offset = offsetof(Vertex, position);	// 自行计算顶点属性中成员变量的偏移

		attributeDescriptions[1].binding = 0;	// 与位置交错绑定
		attributeDescriptions[1].location = 1; // 对应vertex shader对应的color
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;	// vec3
		attributeDescriptions[1].offset = offsetof(Vertex, color);
		return attributeDescriptions;
	}

	void CzxModel::Builder::loadModel(const std::string& filePath) {
		// 使用 rapidobj 解析 OBJ 文件
		rapidobj::Result result = rapidobj::ParseFile(filePath);

		if (result.error) {
			throw std::runtime_error("Failed to load OBJ file: " + filePath +
				"\nError: " + result.error.line);
		}

		// 获取属性数据引用
		auto& positions = result.attributes.positions;
		auto& normals = result.attributes.normals;
		auto& texcoords = result.attributes.texcoords;

		// 清空现有数据
		vertices.clear();
		indices.clear();

		std::unordered_map<Vertex, uint32_t> uniqueVertices{};	// 去重vertices得到indices
		// 遍历所有形状（mesh 组）
		for (const auto& shape : result.shapes) {
			// 遍历该形状的所有索引（三角形顶点）
			for (const auto& index : shape.mesh.indices) {
				Vertex vertex{};

				// 位置
				if (index.position_index >= 0) {
					vertex.position = {
						positions[3 * index.position_index + 0],
						1.f - positions[3 * index.position_index + 1],
						positions[3 * index.position_index + 2]
					};
				}
				else {
					vertex.position = { 0.0f, 0.0f, 0.0f };
				}

				// 法线
				if (index.normal_index >= 0) {
					vertex.normal = {
						normals[3 * index.normal_index + 0],
						normals[3 * index.normal_index + 1],
						normals[3 * index.normal_index + 2]
					};
				}
				else {
					// 如果没有法线，使用默认值
					vertex.normal = { 0.0f, 0.0f, 1.0f };
				}

				// 纹理坐标
				if (index.texcoord_index >= 0) {
					vertex.uv = {
						texcoords[2 * index.texcoord_index + 0],
						1.0f - texcoords[2 * index.texcoord_index + 1]  // Vulkan 需要翻转 Y 轴
					};
				}
				else {
					vertex.uv = { 0.0f, 0.0f };
				}

				// 临时颜色
				vertex.color = vertex.normal;

				if (uniqueVertices.count(vertex) == 0) {
					uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
					vertices.push_back(vertex);
				}
				indices.push_back(uniqueVertices[vertex]);
			}
		}

		// 输出加载信息
		std::cout << "Loaded model: " << filePath << std::endl;
		std::cout << "  Vertices: " << vertices.size() << std::endl;
		std::cout << "  Triangles: " << indices.size() / 3 << std::endl;
		std::cout << "  Materials: " << result.materials.size() << std::endl;

		// 默认加载
		//Loaded model: D:/PG/RealTimeEngine/models/WhiteWeddingDressGirl/WhiteWeddingDressGirl.obj
		//Vertices: 2917794
		//Triangles : 972598
		//Materials : 1
		//Vertex count : 2917794

		// hash去重vertices与indices
		// Loaded model: D:/PG/RealTimeEngine/models/WhiteWeddingDressGirl/WhiteWeddingDressGirl.obj
		// Vertices: 530843
		// Triangles : 972598
		// Materials : 1
		// Vertex count : 530843
	}
}	// namespace czx