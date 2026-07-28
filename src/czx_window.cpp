#include <czx_window.h>

// std
#include <stdexcept>

namespace czx {

	CzxWindow::CzxWindow(int w, int h, std::string name)
		:m_width(w), m_height(h), m_windowName(name)
	{
		initWindow();
	}

	CzxWindow::~CzxWindow() {
		glfwDestroyWindow(m_window);
		glfwTerminate();
	}

	void CzxWindow::initWindow() {
		glfwInit();	// 初始化glfw

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);	// 禁止glfw使用后续调用来创建opengl上下文
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);		// 禁止窗口调整大小

		m_window = glfwCreateWindow(m_width, m_height, m_windowName.c_str(), nullptr, nullptr);
	}

	void CzxWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface) {
		if (glfwCreateWindowSurface(instance, m_window, nullptr, surface) != VK_SUCCESS) {
			throw std::runtime_error("failed to create window surface");
		}
	}

}	// namespace czx