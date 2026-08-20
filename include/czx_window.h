#pragma once

// 使用glfw3内的vulkan
#define GLFW_INCLUDE_VULKAN  // 让GLFW在包含时同步引入Vulkan相关类型声明，后续代码可以直接使用Vulkan接口。
#include <GLFW/glfw3.h>  // 引入GLFW窗口系统头文件，提供窗口创建、事件处理和表面创建能力。

// std
#include <string>  // 引入std::string类型，用于保存窗口标题等字符串信息。

namespace czx {

	// 该类封装了GLFW窗口对象以及与Vulkan窗口表面相关的状态，负责窗口的创建、尺寸查询和资源清理。
	class CzxWindow {
	public:
		// 初始化窗口
		CzxWindow(int w, int h, std::string name);  // 构造函数接收窗口宽高和标题，用于创建一个新的窗口实例。
		// 清理GLFW资源
		~CzxWindow();  // 析构函数在对象销毁时释放窗口和GLFW运行时相关资源。

		// 禁止复制窗口对象，避免多个对象管理一个GLFW窗口指针，造成悬空指针和重复释放
		CzxWindow(const CzxWindow&) = delete;  // 禁止拷贝构造，避免多个对象同时持有同一个窗口句柄。
		CzxWindow& operator=(const CzxWindow&) = delete;  // 禁止拷贝赋值，防止重复释放同一个GLFW窗口资源。

		// 检查窗口是否应该关闭(关闭按钮)(可推展到ImGui的按键关闭)
		bool shouldClose() { return glfwWindowShouldClose(m_window); }  // 通过GLFW判断窗口是否已经收到关闭请求。
		// 获取当前窗口尺寸
		VkExtent2D getExtent() { return { static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height) }; }  // 将当前窗口尺寸转换成Vulkan交换链所需的范围结构。

		GLFWwindow* getGLFWwindow() const { return m_window; }

		// 用于重建交换链：检查窗口是否被调整大小，重置标志
		bool wasWindowResized() { return m_framebufferResized; }  // 返回窗口是否发生过尺寸变化，供外部决定是否重建交换链。
		void resetWindowResizedFlag() { m_framebufferResized = false; }  // 重置尺寸变化标志，避免重复触发重建逻辑。
		bool isValid() const { return m_window != nullptr; }

		// 创建vulkan窗口表面，vulkan依赖于表面来呈现图像
		void createWindowSurface(VkInstance instance, VkSurfaceKHR* surface);  // 为当前窗口创建Vulkan呈现表面，供渲染和交换链使用。
	private:
		// esc按键关闭窗口回调函数
		static void GLFW_KeyCallBack(GLFWwindow* window, int key, int scancode, int action, int mods);
		
		// 帧缓冲大小变化的回调函数(C风格静态函数)，即窗口大小变化时自动调用来更新尺寸
		static void GLFW_FramebufferResizeCallBack(GLFWwindow* window, int width, int height);  // 这是GLFW回调函数入口，用于在窗口大小变化时同步更新内部状态。
		// 内部初始化窗口的接口，用于构造中调用
		void initWindow();  // 把GLFW窗口创建与初始化流程封装在这里，构造函数只需调用它即可。

		int m_width;	// 窗口宽度
		int m_height;	// 窗口高度
		bool m_framebufferResized = false;  // 记录窗口帧缓冲尺寸是否发生变化，便于外部判断是否需要重建资源。

		// 管理窗口对象与窗口名称
		std::string m_windowName;  // 保存窗口标题字符串，创建窗口时会用它作为显示名称。
		GLFWwindow* m_window;  // 保存GLFW窗口句柄，后续所有窗口操作都基于这个指针。
	};
}	// namespace czx