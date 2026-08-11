#include <first_app.h>

#include <czx_camera.h>
#include <czx_buffer.h>
#include <czx_frame_info.h>
#include <simple_render_system.h>
#include <keyboard_movement_controller.h>

// libs
// 弧度制
#define GLM_FORCE_RADIANS
// 限定深度缓冲在[0,1]
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

//std
#include <chrono>
#include <stdexcept>
#include <array>

namespace czx {

	struct GlobalUbo{
		alignas(16) glm::mat4 projectionView{ 1.f };
		alignas(16) glm::vec3 directionToLight = glm::normalize(glm::vec3(3.0, -3.0, 2.0));
	};

	// 构造函数会在应用启动时加载初始场景对象。
	FirstAPP::FirstAPP() {
		m_globalPool = CzxDescriptorPool::Builder(m_device)
			.setMaxSets(CzxSwapChain::MAX_FRAMES_IN_FLIGHT)
			.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, CzxSwapChain::MAX_FRAMES_IN_FLIGHT)
			.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, CzxSwapChain::MAX_FRAMES_IN_FLIGHT + 10)
			.build();
		loadGameObjects();
	}

	// 默认析构函数保持空实现，资源由成员对象在其自身生命周期中释放。
	FirstAPP::~FirstAPP() {}

	// 主循环会持续处理窗口事件，并在每一帧中提交渲染命令。
	void FirstAPP::run() {
		std::vector<std::unique_ptr<CzxBuffer>> uboBuffers(CzxSwapChain::MAX_FRAMES_IN_FLIGHT);
		for (int i = 0; i < uboBuffers.size(); ++i) {
			uboBuffers[i] = std::make_unique<CzxBuffer>(
				m_device,
				sizeof(GlobalUbo),
				1,
				VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			uboBuffers[i]->map();
		}

		auto descriptorSetLayouts = CzxDescriptorSetLayout::Builder(m_device)
			.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)	// globalUbo
			.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)	// imageSampler
			.build();

		std::vector<VkDescriptorSet> descriptorSets(CzxSwapChain::MAX_FRAMES_IN_FLIGHT);

		std::shared_ptr<CzxTexture> sharedTexture;
		for (auto& obj : m_gameObjects) {
			if (obj.m_model && obj.m_model->hasTexture()) {
				sharedTexture = obj.m_model->getTexture();
				break;
			}
		}
		if (!sharedTexture) {
			throw std::runtime_error("failed to load texture");
		}
		VkDescriptorImageInfo imageInfo = sharedTexture->getDescriptorInfo();

		for (int i = 0; i < descriptorSets.size(); ++i) {
			auto bufferInfo = uboBuffers[i]->descriptorInfo();
			CzxDescriptorWriter(*descriptorSetLayouts, *m_globalPool)
				.writeBuffer(0, &bufferInfo)
				.writeImage(1, &imageInfo)
				.build(descriptorSets[i]);
		}

		std::vector<VkDescriptorSetLayout> setLayouts{
			descriptorSetLayouts->getDescriptorSetLayout()
		};
		SimpleRenderSystem simpleRenderSystem{ m_device, m_renderer.getSwapChainRenderPass(), setLayouts };
        CzxCamera camera{};

        auto viewObject = CzxGameObject::createGameObject();    // 仅用于观测，无实体
        KeyboardMovementController cameraController{};

        auto currentTime = std::chrono::high_resolution_clock::now();

		while (!m_window.shouldClose()) {
			glfwPollEvents();	// 检查处理所有窗口事件

            // 处理事件可能阻塞
            auto newTime = std::chrono::high_resolution_clock::now();
            auto frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;

            cameraController.moveInPlaneXZ(m_window.getGLFWwindow(), frameTime, viewObject);
            camera.setViewYXZ(viewObject.m_transform.translation, viewObject.m_transform.rotation);

            float aspect = m_renderer.getAspectRatio();
            camera.setPersepctiveProjection(glm::radians(50.f), aspect, 0.1f, 10.f);

			if (auto commandBuffer = m_renderer.beginFrame()) {
				int frameIndex = m_renderer.getFrameIndex();
				FrameInfo frameInfo{
					frameIndex,
					frameTime,
					commandBuffer,
					camera,
					descriptorSets[frameIndex]
				};

				// update
				GlobalUbo ubo{};
				ubo.projectionView = camera.getProjection() * camera.getView();
				uboBuffers[frameIndex]->writeToBuffer(&ubo);
				uboBuffers[frameIndex]->flush();

				// render
				m_renderer.beginSwapChainRenderPass(commandBuffer);
				simpleRenderSystem.renderGameObjects(frameInfo, m_gameObjects);
				m_renderer.endSwapChainRenderPass(commandBuffer);
				m_renderer.endFrame();
			}
		}

		vkDeviceWaitIdle(m_device.device());
	}

	// 将需要展示的模型实例与相关变换数据加载到场景中。
	void FirstAPP::loadGameObjects() {
        std::shared_ptr<CzxModel> model = CzxModel::createModelFromFile(m_device, "D:/PG/RealTimeEngine/models/WhiteWeddingDressGirl/WhiteWeddingDressGirl.obj",
			"D:/PG/RealTimeEngine/models/WhiteWeddingDressGirl/WhiteWeddingDressGirl.fbm/WhiteWeddingDressGirl_01_basecolor.jpg");

        auto gameObj = CzxGameObject::createGameObject();
		gameObj.m_model = model;
		gameObj.m_transform.translation = { .0f,-1.f, 2.5f };
		gameObj.m_transform.scale = { 2.f,2.f, 2.f };
        m_gameObjects.push_back(std::move(gameObj));

	}
}	// namespace czx