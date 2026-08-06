#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 uv;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragUv;

// 推送的常量数据顺序要一致
layout(push_constant) uniform Push{
	mat4 modelMatrix;
	mat4 normalMatrix;
} push;

layout(set = 0, binding = 0) uniform Globalubo{
	mat4 projectionViewMatrix;
	vec3 directionToLight;
} ubo;

const float AMBIENT = 0.02;	// 简单模拟漫反射

// 每个顶点都要运行
void main(){
	gl_Position = ubo.projectionViewMatrix * push.modelMatrix * vec4(position, 1.f);

	// 1.仅在均匀缩放时正确
	// vec3 normalWorldSpace = normalize(mat3(push.modelMatrix) * normal);

	// 2.逐顶点计算逆转置矩阵	开销很大
	//mat3 normalMatrix = transpose(inverse(mat3(push.modelMatrix)));
	//vec3 normalWorldSpace = normalize(normalMatrix * normal);

	// 3.在host计算normalMatrix后传入shader
	vec3 normalWorldSpace = normalize(mat3(push.normalMatrix) * normal);

	float lightIntensity = clamp((AMBIENT + max(dot(normalWorldSpace, ubo.directionToLight), 0)),0.f, 1.f);

	fragColor = lightIntensity * color;

	fragUv = uv;
}