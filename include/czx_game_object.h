#pragma once

#include <czx_model.h>

//lib
#include <glm/gtc/matrix_transform.hpp>
// std
#include <memory>


namespace czx {

	// 3D变换组件，描述对象的位置、缩放和旋转，用于生成模型变换矩阵。
	struct TransformComponent {
		glm::vec3 translation{};  // 位置偏移量，表示对象在三维空间中的平移。
		glm::vec3 scale{1.f,1.f, 1.f};  // 缩放因子，控制对象在XYZ轴上的缩放大小。
		glm::vec3 rotation{};  // 朝向(旋转角度)，单位为弧度。

		//// translate * Ry * Rx * Rz * scale
		//glm::mat4 mat4() {
		//	
		//	auto transform = glm::translate(glm::mat4{ 1.f }, translation);
		//	//y x z欧拉角
		//	transform = glm::rotate(transform, rotation.y, { 0.f, 1.f, 0.f });
		//	transform = glm::rotate(transform, rotation.x, { 1.f, 0.f, 0.f });
		//	transform = glm::rotate(transform, rotation.z, { 0.f, 0.f, 1.f });
		//	transform = glm::scale(transform, scale); // = transform * scale
		//	return transform;
		//}


		// Matrix corrsponds to Translate * Ry * Rx * Rz * Scale
		  // Rotations correspond to Tait-bryan angles of Y(1), X(2), Z(3)
		  // https://en.wikipedia.org/wiki/Euler_angles#Rotation_matrix
		glm::mat4 mat4() {
			const float c3 = glm::cos(rotation.z);
			const float s3 = glm::sin(rotation.z);
			const float c2 = glm::cos(rotation.x);
			const float s2 = glm::sin(rotation.x);
			const float c1 = glm::cos(rotation.y);
			const float s1 = glm::sin(rotation.y);
			return glm::mat4{
				{
					scale.x * (c1 * c3 + s1 * s2 * s3),
					scale.x * (c2 * s3),
					scale.x * (c1 * s2 * s3 - c3 * s1),
					0.0f,
				},
				{
					scale.y * (c3 * s1 * s2 - c1 * s3),
					scale.y * (c2 * c3),
					scale.y * (c1 * c3 * s2 + s1 * s3),
					0.0f,
				},
				{
					scale.z * (c2 * s1),
					scale.z * (-s2),
					scale.z * (c1 * c2),
					0.0f,
				},
				{translation.x, translation.y, translation.z, 1.0f} };
		}
	};

	// 表示场景中的一个游戏对象，包含模型、颜色和变换信息，便于渲染系统统一处理。
	class CzxGameObject {
	public:
		using id_t = unsigned int;

		static CzxGameObject createGameObject() {  // 创建一个带自动递增ID的游戏对象实例。
			static id_t currentId = 0;
			return CzxGameObject(currentId++);
		}

		CzxGameObject(const CzxGameObject&) = delete;
		CzxGameObject& operator=(const CzxGameObject&) = delete;
		CzxGameObject(CzxGameObject&&) = default;
		CzxGameObject& operator=(CzxGameObject&&) = default;

		id_t getId()const { return m_id; }  // 返回对象的唯一ID，便于识别和管理。

		std::shared_ptr<CzxModel> m_model{};  // 指向要渲染的模型数据。
		glm::vec3 m_color{};  // 控制对象的基础颜色。
		TransformComponent m_transform{};  // 3D变换信息，用于生成绘制时的模型变换。
	private:
		CzxGameObject(id_t objId) : m_id{objId} {}

		id_t m_id;
	};
}	// namespace czx