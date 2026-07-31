#version 450

layout(location = 0) in vec2 position;
layout(location = 1) in vec3 color;

// 推送的常量数据顺序要一致
layout(push_constant) uniform Push{
	mat2 transform;
	vec2 offset;
	vec3 color;
} push;


// 每个顶点都要运行
void main(){
	gl_Position = vec4(push.transform * position + push.offset, 0.0, 1.0);
}