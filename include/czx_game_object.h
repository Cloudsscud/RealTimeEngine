#pragma once

#include <czx_model.h>

// std
#include <memory>

namespace czx {

	struct Transform2dComponent {
		glm::vec2 transform{};		// position offset
		glm::vec2 scale{1.f,1.f};	// position scale
		float rotation{};			// rotation radians

		glm::mat2 mat2() {
			const float c = glm::cos(rotation);
			const float s = glm::sin(rotation);

			glm::mat2 rotationMat{ {c, s}, {-s, c} };
			glm::mat2 scaleMat{ {scale.x, 0.0f}, {0.0f, scale.y} };
			return rotationMat * scaleMat;
		}
	};

	class CzxGameObject {
	public:
		using id_t = unsigned int;

		static CzxGameObject createGameObject() {
			static id_t currentId = 0;
			return CzxGameObject(currentId++);
		}

		CzxGameObject(const CzxGameObject&) = delete;
		CzxGameObject& operator=(const CzxGameObject&) = delete;
		CzxGameObject(CzxGameObject&&) = default;
		CzxGameObject& operator=(CzxGameObject&&) = default;

		id_t getId()const { return m_id; }

		std::shared_ptr<CzxModel> m_model{};
		glm::vec3 m_color{};
		Transform2dComponent m_transform2d{};
	private:
		CzxGameObject(id_t objId) : m_id{objId} {}

		id_t m_id;
	};
}	// namespace czx