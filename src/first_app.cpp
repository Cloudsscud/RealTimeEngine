#include <first_app.h>

namespace czx {
	void FirstAPP::run() {
		while (!m_window.shouldClose()) {
			glfwPollEvents();	// 检查处理所有窗口事件

		}
	}
}	// namespace czx