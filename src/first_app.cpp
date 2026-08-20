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

	FirstAPP::FirstAPP() {
		loadGameObjects();
	}

	// 默认析构函数保持空实现，资源由成员对象在其自身生命周期中释放。
	FirstAPP::~FirstAPP() {}

	// 主循环会持续处理窗口事件，并在每一帧中提交渲染命令。
	void FirstAPP::run() {

		SimpleRenderSystem simpleRenderSystem{ m_device, m_renderer.getSwapChainRenderPass() };
        CzxCamera camera{};

        auto viewObject = CzxGameObject::createGameObject();    // 仅用于观测，无实体
        KeyboardMovementController cameraController{};

        auto currentTime = std::chrono::high_resolution_clock::now();

		while (!m_window.shouldClose()) {
			glfwPollEvents();	// 检查处理所有窗口事件

			// 计算帧时间
            auto newTime = std::chrono::high_resolution_clock::now();
            auto frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;

			// 更新相机
            cameraController.moveInPlaneXZ(m_window.getGLFWwindow(), frameTime, viewObject);
            camera.setViewYXZ(viewObject.m_transform.translation, viewObject.m_transform.rotation);
            float aspect = m_renderer.getAspectRatio();
            camera.setPersepctiveProjection(glm::radians(50.f), aspect, 0.1f, 10.f);

			// 开始帧
			if (auto commandBuffer = m_renderer.beginFrame()) {
				int frameIndex = m_renderer.getFrameIndex();
				FrameInfo frameInfo{
					frameIndex,
					frameTime,
					commandBuffer,
					camera
				};

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

        auto gameObj2 = CzxGameObject::createGameObject();
		gameObj2.m_model = model;
		gameObj2.m_transform.translation = { .0f,0.f, 3.5f };
		gameObj2.m_transform.scale = { 1.f,1.f, 1.f };
        m_gameObjects.push_back(std::move(gameObj2));

	}
}	// namespace czx