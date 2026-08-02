#include <czx_window.h>

// std
#include <stdexcept>	// runtime_error

namespace czx {

	// 构造函数会把窗口尺寸和标题保存到成员变量中，然后调用初始化流程创建真正的窗口对象。
	CzxWindow::CzxWindow(int w, int h, std::string name)
		:m_width(w), m_height(h), m_windowName(name)  // 通过初始化列表把构造参数同步到成员变量，后续窗口操作会直接使用这些值。
	{
		initWindow();  // 调用内部初始化函数，完成GLFW窗口创建和回调注册。
	}

	// 析构函数负责释放窗口句柄和GLFW全局资源，避免程序退出前留下未释放的对象。
	CzxWindow::~CzxWindow() {
		glfwDestroyWindow(m_window);  // 释放当前GLFW窗口句柄占用的资源。
		glfwTerminate();  // 结束GLFW库的使用，释放其全局运行时资源。
	}

	// 初始化函数把GLFW窗口的基本配置和创建过程封装起来，构造函数只需调用它即可。
	void CzxWindow::initWindow() {
		glfwInit();  // 初始化GLFW运行环境，确保窗口系统和事件系统可用。

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // 告诉GLFW不要为当前窗口创建OpenGL上下文，因为本项目使用Vulkan。
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);  // 允许窗口被拖拽改变大小，方便运行时响应用户调整窗口。

		m_window = glfwCreateWindow(m_width, m_height, m_windowName.c_str(), nullptr, nullptr);  // 根据宽高和标题创建一个新的GLFW窗口。
		glfwSetWindowUserPointer(m_window, this);  // 把当前CzxWindow对象指针绑定到窗口中，供回调函数使用。
		glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallBack);  // 注册尺寸变化回调，让窗口大小改变时自动更新内部状态。
	}

	// 该函数把当前窗口连接到Vulkan，生成可用于呈现的表面对象。
	void CzxWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface) {
		if (glfwCreateWindowSurface(instance, m_window, nullptr, surface) != VK_SUCCESS) {  // 调用GLFW为当前窗口创建Vulkan表面，并检查返回结果。
			throw std::runtime_error("failed to create window surface");  // 抛出运行时异常，提示窗口表面创建失败。
		}
	}
	
	// 这是GLFW在窗口帧缓冲尺寸变化时自动触发的回调函数，用于同步当前窗口尺寸状态。
	void CzxWindow::framebufferResizeCallBack(GLFWwindow* window, int width, int height) {
		auto czxWindow = reinterpret_cast<CzxWindow*>(window);  // 从GLFW窗口句柄反向转换为当前窗口对象实例，便于更新成员变量。
		czxWindow->m_framebufferResized = true;  // 标记窗口尺寸已经发生变化，供上层逻辑判断是否需要重建交换链。
		czxWindow->m_width = width;  // 更新保存的窗口宽度，保持与实际帧缓冲尺寸一致。
		czxWindow->m_height = height;  // 更新保存的窗口高度，保持与实际帧缓冲尺寸一致。
	}


}	// namespace czx