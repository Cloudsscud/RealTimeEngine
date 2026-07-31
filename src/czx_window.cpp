#include <czx_window.h>

// std
#include <stdexcept>	// runtime_error

namespace czx {

	CzxWindow::CzxWindow(int w, int h, std::string name)
		:m_width(w), m_height(h), m_windowName(name)
	{
		initWindow();
	}

	CzxWindow::~CzxWindow() {
		// 销毁窗口对象
		glfwDestroyWindow(m_window);
		// 终止GLFW上下文
		glfwTerminate();
	}

	void CzxWindow::initWindow() {
		// 初始化glfw上下文
		glfwInit();

		// 设置窗口提示
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);	// 禁止glfw使用后续调用来创建opengl上下文，vulkan用不到opengl上下文
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);		// 允许窗口调整大小，需重建交换链

		// 创建GLFW窗口对象
		m_window = glfwCreateWindow(m_width, m_height, m_windowName.c_str(), nullptr, nullptr);
		// 使用回调函数之前需要设置对应的GLFW用户指针,用于在回调中获取对应对象实例的上下文信息
		glfwSetWindowUserPointer(m_window, this);
		// 为帧缓冲大小变化设置回调，确保在窗口大小变化时自动调用该回调函数
		glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallBack);
	}

	// 显示图像需要窗口表面; 创建vulkan窗口表面，其依赖于device创建的vulkan实例
	void CzxWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface) {
		if (glfwCreateWindowSurface(instance, m_window, nullptr, surface) != VK_SUCCESS) {
			throw std::runtime_error("failed to create window surface");
		}
	}

	// 帧缓冲大小变化回调函数，处理窗口大小变化的上下文细节；将上下文获取到的GLFW窗口指针转换回便于管理的对象指针，然后对对象指针的相关内容修改(对指针的内容修改)
	void CzxWindow::framebufferResizeCallBack(GLFWwindow* window, int width, int height) {
		auto czxWindow = reinterpret_cast<CzxWindow*>(window);
		czxWindow->m_framebufferResized = true;
		czxWindow->m_width = width;
		czxWindow->m_height = height;
	}


}	// namespace czx