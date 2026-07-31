#pragma once

#include <czx_window.h>
#include <czx_device.h>
#include <czx_game_object.h>
#include <czx_renderer.h>

// std
#include <memory>
#include <vector>

namespace czx {
	class FirstAPP {
	public:
		static constexpr int WIDTH = 800;
		static constexpr int HEIGHT = 600;

		FirstAPP();
		~FirstAPP();

		FirstAPP(const FirstAPP&) = delete;
		FirstAPP& operator=(const FirstAPP&) = delete;

		void run();
	private:
		void loadGameObjects();

		CzxWindow m_window{WIDTH, HEIGHT, "Hello Vulkan!"};
		CzxDevice m_device{m_window};
		CzxRenderer m_renderer{ m_window, m_device };

		std::vector<CzxGameObject> m_gameObjects;
	};
}	// namespace czx