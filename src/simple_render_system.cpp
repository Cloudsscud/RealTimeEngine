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
#include <cassert>

namespace czx {
	// 推送常量数据结构，用来把每个对象的变换和颜色传递给着色器，最大128bytes=2*mat4x4
	struct SimplePushConstantData {
		glm::mat4 modelMatrix{ 1.f };
		glm::mat4 normalMatrix{ 1.f };
	};

	// 构造函数会先创建管线布局，再根据当前渲染通道创建具体的图形管线。
	SimpleRenderSystem::SimpleRenderSystem(CzxDevice& device, VkRenderPass renderPass)
		:m_device{device}
	{

		createDescriptorSetLayouts();
		createDescriptorPools();
		allocateGlobalDescriptorSets();
		createPipelineLayout();
		createPipeline(renderPass);
	}

	// 析构函数释放管线布局资源，避免内存泄漏。
	SimpleRenderSystem::~SimpleRenderSystem() {
		vkDestroyPipelineLayout(m_device.device(), m_pipelineLayout, nullptr);
	}

	// 描述符布局
	void SimpleRenderSystem::createDescriptorSetLayouts() {
		// 全局布局：binding 0 = Uniform Buffer
		CzxDescriptorSetLayout::Builder globalBuilder(m_device);
		globalBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
		m_globalLayout = globalBuilder.build();

		// 模型布局：binding 0 = Combined Image Sampler (漫反射纹理)
		// 可扩展更多 binding，例如 1 = 法线纹理，2 = 粗糙度纹理，3 = 金属度纹理等
		CzxDescriptorSetLayout::Builder modelBuilder(m_device);
		modelBuilder.addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		// 若需多纹理，在此添加更多 addBinding
		m_modelLayout = modelBuilder.build();
	}

	// 描述符池
	void SimpleRenderSystem::createDescriptorPools() {
		// 全局池：为每个飞行帧分配一个描述符集，每个集包含一个 UBO
		CzxDescriptorPool::Builder globalPoolBuilder(m_device);
		globalPoolBuilder.setMaxSets(MAX_FRAMES_IN_FLIGHT)
			.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT);
		m_globalPool = globalPoolBuilder.build();

		// 模型池：预设最大对象数量
		const uint32_t maxObjects = 100;
		CzxDescriptorPool::Builder modelPoolBuilder(m_device);
		modelPoolBuilder.setMaxSets(maxObjects)
			.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxObjects);	// 预分配大量模型纹理空间，等待模型请求使用
		m_modelPool = modelPoolBuilder.build();
	}

	// 分配全局描述符集
	void SimpleRenderSystem::allocateGlobalDescriptorSets() {
		m_globalDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
		m_uboBuffers.resize(MAX_FRAMES_IN_FLIGHT);

		VkDeviceSize bufferSize = sizeof(GlobalUbo);
		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			// 创建 UBO 缓冲（主机可见、一致，保持映射状态）
			m_uboBuffers[i] = std::make_unique<CzxBuffer>(
				m_device,
				bufferSize,
				1,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
			);
			m_uboBuffers[i]->map(); // 永久映射，方便每帧更新

			// 分配并写入全局描述符集（绑定该 UBO 缓冲）
			CzxDescriptorWriter writer(*m_globalLayout, *m_globalPool);
			VkDescriptorBufferInfo bufferInfo = m_uboBuffers[i]->descriptorInfo();
			writer.writeBuffer(0, &bufferInfo);
			std::vector<VkDescriptorSet> sets;
			bool success = writer.build(sets, 1);
			assert(success && "Failed to allocate global descriptor set");
			m_globalDescriptorSets[i] = sets[0];
		}
	}

	// 分配模型描述符集（延迟分配）
	void SimpleRenderSystem::allocateModelDescriptorSet(const CzxGameObject& obj) {
		// 若已分配则直接返回
		if (m_modelDescriptorSets.find(obj.getId()) != m_modelDescriptorSets.end())
			return;

		// 检查模型是否有效且包含纹理
		if (!obj.m_model || !obj.m_model->hasTexture()) {
			return;
		}

		// 写入纹理信息到模型布局的 binding 0
		CzxDescriptorWriter writer(*m_modelLayout, *m_modelPool);
		VkDescriptorImageInfo imageInfo = obj.m_model->getTexture()->getDescriptorInfo();
		writer.writeImage(0, &imageInfo);
		std::vector<VkDescriptorSet> sets;
		bool success = writer.build(sets, 1);
		assert(success && "Failed to allocate model descriptor set");

		m_modelDescriptorSets[obj.getId()] = sets[0];
	}

	// 更新 UBO 内容
	void SimpleRenderSystem::updateUbo(int frameIndex, const CzxCamera& camera) {

		GlobalUbo ubo{};
		ubo.projectionView = camera.getProjection() * camera.getView();

		m_uboBuffers[frameIndex]->writeToBuffer(&ubo);
		// 若内存非 coherent，需 flush，但这里使用 coherent 内存，可省略
		// m_uboBuffers[frameIndex]->flush();
	}

	// 为当前渲染系统创建管线布局，配置描述符集与推送常量信息
	void SimpleRenderSystem::createPipelineLayout() {
		// 使用两个描述符集布局：set=0 全局，set=1 模型
		std::vector<VkDescriptorSetLayout> layouts = {
			m_globalLayout->getDescriptorSetLayout(),
			m_modelLayout->getDescriptorSetLayout()
		};

		VkPushConstantRange pushConstantRange{
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,	// 使用推送常量的着色器阶段
			.offset = 0,	// 使用的起始偏移
			.size = sizeof(SimplePushConstantData)	// 推送常量大小
		};

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = static_cast<uint32_t>(layouts.size()),	// 该管线使用到的描述符集布局信息
			.pSetLayouts = layouts.data(),
			.pushConstantRangeCount = 1,	// 该管线使用到的推送常量信息
			.pPushConstantRanges = &pushConstantRange
		};

		VkResult result = vkCreatePipelineLayout(m_device.device(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout);
		CHECK_VK_RESULT(result, "create pipeline layout\n");
	}

	// 使用当前渲染通道创建图形管线，后续绘制会使用这条管线。
	void SimpleRenderSystem::createPipeline(VkRenderPass renderPass) {
		assert(m_pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

		PipelineConfigInfo pipelineConfig{};
		CzxPipeline::defaultPipelineConfigInfo(pipelineConfig);
		pipelineConfig.pipelineLayout = m_pipelineLayout;
		pipelineConfig.renderPass = renderPass;
		m_pipeline = std::make_unique<CzxPipeline>(m_device, "shaders/simple_shader.vert.spv", "shaders/simple_shader.frag.spv", pipelineConfig);
	}

	// 遍历所有游戏对象，更新其变换并依次绑定顶点缓冲和提交绘制调用。
	void SimpleRenderSystem::renderGameObjects(FrameInfo& frameInfo, std::vector<CzxGameObject>& gameObjects) {
		updateUbo(frameInfo.frameIndex, frameInfo.camera);

		// 2. 绑定全局描述符集（set=0）
		vkCmdBindDescriptorSets(
			frameInfo.commandBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			m_pipelineLayout,
			0,                     // first set
			1,
			&m_globalDescriptorSets[frameInfo.frameIndex],
			0,		// 动态偏移量
			nullptr
		);

		// render
		// bind vertex buffer后，drawCall之前，推送常量
		//多个不同常量图形便多次pushConstant 并 drawCall
		//将多个模型绑定放在一个缓冲区内可以减少大量的drawCall可以优化性能
		//但是此时不允许每个模型访问各自特有的推送常量
		m_pipeline->bind(frameInfo.commandBuffer);

		for (auto& obj : gameObjects) {
			// 确保模型描述符集已分配（首次遇到时分配）
			allocateModelDescriptorSet(obj);

			// 绑定模型描述符集（set=1）纹理
			vkCmdBindDescriptorSets(
				frameInfo.commandBuffer,
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				m_pipelineLayout,
				1,                     // first set
				1,
				&m_modelDescriptorSets[obj.getId()],
				0,
				nullptr
			);

			// 推送常量：模型矩阵和法线矩阵
			SimplePushConstantData push{};
			static float foo = 0.0f;
			push.modelMatrix = glm::rotate(obj.m_transform.mat4(), glm::radians(foo), glm::normalize(glm::vec3(0.0f, 1.f, 0.0f)));
			push.normalMatrix = glm::rotate(glm::mat4{obj.m_transform.normalMatrix()}, glm::radians(foo), glm::normalize(glm::vec3(0.0f, 1.f, 0.0f)));
			foo += 0.01f;

			vkCmdPushConstants(
				frameInfo.commandBuffer,
				m_pipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(SimplePushConstantData), &push);

			// 绑定并绘制模型
			obj.m_model->bind(frameInfo.commandBuffer);
			obj.m_model->draw(frameInfo.commandBuffer);
		}
	}


}	// namespace czx