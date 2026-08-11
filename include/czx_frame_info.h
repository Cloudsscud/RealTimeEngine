#pragma once

#include <czx_camera.h>

// lib
#include <vulkan/vulkan.h>

namespace czx {
	struct FrameInfo {
		int frameIndex;
		float frameTime;
		VkCommandBuffer commandBuffer;
		CzxCamera& camera;
		VkDescriptorSet globalDescriptorSet;
	};
}	// namespace czx