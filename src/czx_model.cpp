#include <czx_model.h>

// std
#include <cassert>
#include <cstring>

namespace czx {
	// 构造函数会把顶点数据上传到GPU，并为后续绘制准备好顶点缓冲区。
	CzxModel::CzxModel(CzxDevice& device, const std::vector<Vertex>& vertices)
		:m_device(device)
	{
		createVertexBuffer(vertices);
	}
	
	// 析构函数释放模型对应的顶点缓冲区和显存资源。
	CzxModel::~CzxModel() {
		vkDestroyBuffer(m_device.device(), m_vertexBuffer, nullptr);
		vkFreeMemory(m_device.device(), m_vertexBufferMemory, nullptr);
	}

	// 创建并填充顶点缓冲区，把CPU端的顶点数据上传到GPU内存中。
	void CzxModel::createVertexBuffer(const std::vector<Vertex>& vertices) {
		m_vertexCount = static_cast<uint32_t>(vertices.size());
		assert(m_vertexCount >= 3 && "Vertex count must be at least 3!");
		VkDeviceSize bufferSize = sizeof(vertices[0]) * m_vertexCount;
		m_device.createBuffer(
			bufferSize,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			m_vertexBuffer,
			m_vertexBufferMemory);

		void* data;
		vkMapMemory(m_device.device(), m_vertexBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, vertices.data(), static_cast<uint32_t>(bufferSize));
		vkUnmapMemory(m_device.device(), m_vertexBufferMemory);
	}
	
	// 把顶点缓冲绑定到当前命令缓冲区，准备在后续绘制调用中使用。
	void CzxModel::bind(VkCommandBuffer commandBuffer) {
		VkBuffer buffers[] = {m_vertexBuffer};
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
	}


	// 发出一次绘制调用，绘制当前绑定的顶点缓冲内容。
	void CzxModel::draw(VkCommandBuffer commandBuffer) {
		vkCmdDraw(commandBuffer, m_vertexCount, 1, 0, 0);
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


}	// namespace czx