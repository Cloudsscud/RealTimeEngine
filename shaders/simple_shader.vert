#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;

layout(location = 0) out vec3 fragColor;

// 推送的常量数据顺序要一致
layout(push_constant) uniform Push{
	mat4 transform;
	vec3 color;
} push;


// 每个顶点都要运行
void main(){
	gl_Position = push.transform * vec4(position, 1.f);
	fragColor = color;
}