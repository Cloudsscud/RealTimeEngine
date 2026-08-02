#pragma once

// 获取键盘输入，用于实时移动对象

#include <czx_game_object.h>
#include <czx_window.h>

namespace czx {
	class KeyboardMovementController{
	public:
		struct KeyMappings {
			// 移动
			int moveLeft = GLFW_KEY_A;
			int moveRight = GLFW_KEY_D;
			int moveForward = GLFW_KEY_W;
			int moveBackward = GLFW_KEY_S;
			int moveUp = GLFW_KEY_E;
			int moveDown = GLFW_KEY_Q;
			// 旋转
			int lookLeft = GLFW_KEY_LEFT;
			int lookRight = GLFW_KEY_RIGHT;
			int lookUp = GLFW_KEY_UP;
			int lookDown = GLFW_KEY_DOWN;
		};

		void moveInPlaneXZ(GLFWwindow* window, float dt, CzxGameObject& gameObject);

		KeyMappings keys{};
		float moveSpeed{ 3.f };	// 移速
		float lookSpeed{ 1.5f };	// 旋转速度
	};
}