#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>

namespace czx {

	class CzxWindow {
	public:
		CzxWindow(int w, int h, std::string name);
		~CzxWindow();

		// Ω˚÷π∏¥÷∆¥∞ø⁄£¨±‹√‚–¸ø’÷∏’Î∫Õ÷ÿ∏¥ Õ∑≈
		CzxWindow(const CzxWindow&) = delete;
		CzxWindow& operator=(const CzxWindow&) = delete;

		bool shouldClose() { return glfwWindowShouldClose(m_window); }
		VkExtent2D getExtent() { return { static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height) }; }
		bool wasWindowResized() { return m_framebufferResized; }
		void resetWindowResizedFlag() { m_framebufferResized = false; }

		void createWindowSurface(VkInstance instance, VkSurfaceKHR* surface);
	private:
		static void framebufferResizeCallBack(GLFWwindow* window, int width, int height);
		void initWindow();

		int m_width;
		int m_height;
		bool m_framebufferResized = false;

		std::string m_windowName;
		GLFWwindow* m_window;
	};
}	// namespace czx