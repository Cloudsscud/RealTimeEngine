#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>

// RAII原则
namespace czx {

	class CzxWindow {
	public:
		CzxWindow(int w, int h, std::string name);
		~CzxWindow();

		// 禁止复制窗口，避免悬空指针和重复释放
		CzxWindow(const CzxWindow&) = delete;
		CzxWindow& operator=(const CzxWindow&) = delete;

		bool shouldClose() { return glfwWindowShouldClose(m_window); }

	private:
		void initWindow();

		// 不初始化数值，消除默认无参构造函数
		const int m_width;
		const int m_height;

		std::string m_windowName;
		GLFWwindow* m_window;
	};
}	// namespace czx