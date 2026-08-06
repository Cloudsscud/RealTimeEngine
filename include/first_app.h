#pragma once

#include <czx_window.h>
#include <czx_device.h>
#include <czx_game_object.h>
#include <czx_renderer.h>
#include <czx_descriptor.h>

// std
#include <memory>
#include <vector>

namespace czx {
	// 这是整个应用的入口类，负责创建窗口、设备、渲染器以及游戏对象，并驱动主循环。
	class FirstAPP {
	public:
		static constexpr int WIDTH = 800;
		static constexpr int HEIGHT = 600;

		FirstAPP();  // 构造函数会初始化窗口和基础渲染资源。
		~FirstAPP();  // 析构函数负责清理应用级对象。

		FirstAPP(const FirstAPP&) = delete;  // 禁止拷贝，避免重复持有同一套渲染资源。
		FirstAPP& operator=(const FirstAPP&) = delete;  // 禁止赋值，避免资源被多次管理。

		void run();  // 运行主循环，处理窗口事件并提交渲染帧。
	private:
		void loadGameObjects();  // 创建并初始化场景中的游戏对象。

		CzxWindow m_window{WIDTH, HEIGHT, "Hello Vulkan!"};  // 创建主窗口，作为整个应用的显示入口。
		CzxDevice m_device{m_window};  // 根据窗口创建Vulkan设备和相关资源。
		CzxRenderer m_renderer{ m_window, m_device };  // 使用设备和窗口管理渲染帧流程。

		std::unique_ptr<CzxDescriptorPool>	m_globalPool{};	// 池应当在设备之前销毁
		std::unique_ptr<CzxDescriptorPool>	m_texturePool{};	// 池应当在设备之前销毁
		std::vector<CzxGameObject> m_gameObjects;  // 保存当前场景中的所有可渲染对象。
	};
}	// namespace czx