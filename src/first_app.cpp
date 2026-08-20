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

// imgui
#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

//std
#include <chrono>
#include <stdexcept>
#include <array>
#include <functional>

namespace czx {

	FirstAPP::FirstAPP() {
		loadGameObjects();

		m_imguiRenderer = std::make_unique<CzxImGuiRenderer>(
			m_device,
			m_window,
			m_renderer.getSwapChainColorFormat(),
			m_renderer.getSwapChainDepthFormat(),
			m_renderer.getSwapChainImageCount()
		);

		// 处理单击的按键事件切换
		m_window.setKeyEventHandler([this](int key, int scancode, int action, int mods) {
			// 注意：这里最好用 action == GLFW_PRESS（按下瞬间）或 GLFW_RELEASE（释放瞬间）
			// 避免按住时重复触发重置、切换等一次性动作
			if (action == GLFW_PRESS) {
				switch (key) {
				case GLFW_KEY_ESCAPE:	// 关闭窗口
					glfwSetWindowShouldClose(m_window.getGLFWwindow(), GLFW_TRUE);
					break;

				case GLFW_KEY_F1: // 切换 ImGui 显示
					m_imguiRenderer->setEnabled(!m_imguiRenderer->isEnabled());
					break;
				}
			}
			});
	}

	// 默认析构函数保持空实现，资源由成员对象在其自身生命周期中释放。
	FirstAPP::~FirstAPP() {}

	// 主循环会持续处理窗口事件，并在每一帧中提交渲染命令。
	void FirstAPP::run() {
		SimpleRenderSystem simpleRenderSystem{ m_device, m_renderer.getSwapChainColorFormat(), m_renderer.getSwapChainDepthFormat()};
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
				m_renderer.beginRendering(commandBuffer);

				simpleRenderSystem.renderGameObjects(frameInfo, m_gameObjects);


				m_imguiRenderer->render(commandBuffer, [&]() {
					SimpleRenderSystem::Settings& settings = simpleRenderSystem.getSettings();

					// 这里编写所有 ImGui 界面代码
					ImGui::Begin("Debug Panel");
					ImGui::Text("FPS: %.1f", 1.0f / frameInfo.frameTime);
					// 调整移速
					if (ImGui::SliderFloat("Camera Move Speed", &cameraController.moveSpeed, 0.5f, 5.0f)) {
						// 值已更新
					}
					ImGui::DragFloat3("Light Dir", &settings.lightDirection[0], 0.1f, -1.0f, 1.0f);
					ImGui::End();

					ImGui::Begin("Model Control");
					ImGui::Checkbox("Enable Rotation", &settings.enableRotation);
					ImGui::SliderFloat("Rotation Speed", &settings.rotationSpeed, 0.0f, 2.0f);
					ImGui::End();
					});

				m_renderer.endRendering(commandBuffer);
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