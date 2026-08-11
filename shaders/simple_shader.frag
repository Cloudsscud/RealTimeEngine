#version 450

layout (location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragUv;

layout (location = 0) out vec4 outColor;

layout(push_constant) uniform Push{
	mat4 modelMatrix;
	mat4 normalMatrix;
} push;

layout(set = 0, binding = 1) uniform sampler2D texSampler;


void main(){
	vec4 texColor = texture(texSampler, fragUv);


	outColor = vec4(fragColor * texColor.rgb, 1.0);
}