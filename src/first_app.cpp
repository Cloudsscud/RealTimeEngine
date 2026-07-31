#include <first_app.h>

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

	FirstAPP::FirstAPP() {
		loadGameObjects();
	}

	FirstAPP::~FirstAPP() {}

	void FirstAPP::run() {
		SimpleRenderSystem simpleRenderSystem{ m_device, m_renderer.getSwapChainRenderPass() };

		while (!m_window.shouldClose()) {
			glfwPollEvents();	// 检查处理所有窗口事件

			if (auto commandBuffer = m_renderer.beginFrame()) {
				m_renderer.beginSwapChainRenderPass(commandBuffer);
				simpleRenderSystem.renderGameObjects(commandBuffer, m_gameObjects);
				m_renderer.endSwapChainRenderPass(commandBuffer);
				m_renderer.endFrame();
			}
		}

		vkDeviceWaitIdle(m_device.device());
	}

	void FirstAPP::loadGameObjects() {
		// {vector{Vertex{glm::vec2}}}
		std::vector<CzxModel::Vertex> vertices{
			{{0.0f,-0.5f}, {1.0f, 0.0f, 0.0f}},
			{{0.5f,0.5f}, { 0.0f, 1.0f, 0.0f }},
			{{-0.5f, 0.5f},{0.0f, 0.0f, 1.0f}}
		};

		auto m_model = std::make_shared<CzxModel>(m_device, vertices);

		for (int i = 0; i < 10; ++i) {
			auto triangle = CzxGameObject::createGameObject();
			triangle.m_model = m_model;
			triangle.m_color = { .1f * i, .8f, 0.1f + 0.01 * i };
			triangle.m_transform2d.transform.x = .2f;
			triangle.m_transform2d.scale = { 1.5f, 0.5f };
			triangle.m_transform2d.rotation = .25f* glm::two_pi<float>();

			m_gameObjects.push_back(std::move(triangle));
		}

	}
}	// namespace czx