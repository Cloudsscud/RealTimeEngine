#include <keyboard_movement_controller.h>

#include <limits>

namespace czx {

	void KeyboardMovementController::moveInPlaneXZ(GLFWwindow* window, float dt, CzxGameObject& gameObject) {

		// 处理旋转
		glm::vec3 rotation{ 0 };
		if (glfwGetKey(window, keys.lookRight) == GLFW_PRESS)	rotation.y += 1.f;
		if (glfwGetKey(window, keys.lookLeft) == GLFW_PRESS)	rotation.y -= 1.f;
		if (glfwGetKey(window, keys.lookUp) == GLFW_PRESS)	rotation.x += 1.f;
		if (glfwGetKey(window, keys.lookDown) == GLFW_PRESS)	rotation.x -= 1.f;


		// 特殊处理无操作导致零异常的情况
		if (glm::dot(rotation, rotation) > std::numeric_limits<float>::epsilon()) {
			gameObject.m_transform.rotation += lookSpeed * dt * glm::normalize(rotation);
		}
		gameObject.m_transform.rotation.x = glm::clamp(gameObject.m_transform.rotation.x, -1.5f, 1.5f);
		gameObject.m_transform.rotation.y = glm::mod(gameObject.m_transform.rotation.y, glm::two_pi<float>());

		float yaw = gameObject.m_transform.rotation.y;
		const glm::vec3 forwardDir{ sin(yaw), 0.f, cos(yaw) };
		const glm::vec3 rightDir{ forwardDir.z, 0.f, -forwardDir.x};
		const glm::vec3 upDir{ 0.f, -1.f, 0.f };	// 屏幕向下为y正

		glm::vec3 moveDir{0};
		if (glfwGetKey(window, keys.moveForward) == GLFW_PRESS)	moveDir += forwardDir;
		if (glfwGetKey(window, keys.moveBackward) == GLFW_PRESS)	moveDir -= forwardDir;
		if (glfwGetKey(window, keys.moveRight) == GLFW_PRESS)	moveDir += rightDir;
		if (glfwGetKey(window, keys.moveLeft) == GLFW_PRESS)	moveDir -= rightDir;
		if (glfwGetKey(window, keys.moveUp) == GLFW_PRESS)	moveDir += upDir;
		if (glfwGetKey(window, keys.moveDown) == GLFW_PRESS)	moveDir -= upDir;

		// 检查未移动
		if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon()) {
			gameObject.m_transform.translation += moveSpeed * dt * glm::normalize(moveDir);
		}
	}

}	// namespace czx