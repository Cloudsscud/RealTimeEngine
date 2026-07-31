#pragma once

// 使用glfw3内的vulkan
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// std
#include <string>

namespace czx {

	class CzxWindow {
	public:
		// 初始化窗口
		CzxWindow(int w, int h, std::string name);
		// 清理GLFW资源
		~CzxWindow();

		// 禁止复制窗口对象，避免多个对象管理一个GLFW窗口指针，造成悬空指针和重复释放
		CzxWindow(const CzxWindow&) = delete;
		CzxWindow& operator=(const CzxWindow&) = delete;

		// 检查窗口是否应该关闭(关闭按钮)(可推展到ImGui的按键关闭)
		bool shouldClose() { return glfwWindowShouldClose(m_window); }
		// 获取当前窗口尺寸
		VkExtent2D getExtent() { return { static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height) }; }

		// 用于重建交换链：检查窗口是否被调整大小，重置标志
		bool wasWindowResized() { return m_framebufferResized; }
		void resetWindowResizedFlag() { m_framebufferResized = false; }

		// 创建vulkan窗口表面，vulkan依赖于表面来呈现图像
		void createWindowSurface(VkInstance instance, VkSurfaceKHR* surface);
	private:
		// 帧缓冲大小变化的回调函数(C风格静态函数)，即窗口大小变化时自动调用来更新尺寸
		static void framebufferResizeCallBack(GLFWwindow* window, int width, int height);
		// 内部初始化窗口的接口，用于构造中调用
		void initWindow();

		int m_width;
		int m_height;
		bool m_framebufferResized = false;	// 检查调整大小的标志，用于动态窗口的调整判断

		// 管理窗口对象与窗口名称
		std::string m_windowName;
		GLFWwindow* m_window;
	};
}	// namespace czx