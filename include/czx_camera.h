#pragma once

// libs
// 弧度制
#define GLM_FORCE_RADIANS
// 限定深度缓冲在[0,1]
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace czx {
	
	class CzxCamera {
	public:
		// 正交投影
		void setOrthographicProjection(float left, float right, float top, float bottom, float near, float far);
		// 透视投影
		void setPersepctiveProjection(float fovy, float aspect, float near, float far);
		// 观察变换
		void setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up = {0.f, -1.f,0.f});	// y朝下	// 观察方向
		void setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up = {0.f, -1.f,0.f});				// 观察目标点
		void setViewYXZ(glm::vec3 position, glm::vec3 rotation);	// 相机旋转

		const glm::mat4& getProjection() const { return m_projectionMatrix; }
		const glm::mat4& getView() const { return m_viewMatrix; }

	private:
		glm::mat4 m_projectionMatrix{ 1.f };
		glm::mat4 m_viewMatrix{ 1.f };
	};

}	// namespace czx