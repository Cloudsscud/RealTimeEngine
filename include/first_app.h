#pragma once

#include <czx_window.h>
#include <czx_device.h>
#include <czx_pipeline.h>

namespace czx {
	class FirstAPP {
	public:
		static constexpr int WIDTH = 800;
		static constexpr int HEIGHT = 600;

		void run();
	private:
		CzxWindow m_window{WIDTH, HEIGHT, "Hello Vulkan!"};
		CzxDevice m_device{m_window};
		CzxPipeline m_pipeline{m_device, "shaders/simple_shader.vert.spv", "shaders/simple_shader.frag.spv", CzxPipeline::defaultPipelineConfigInfo(WIDTH, HEIGHT)};
	};
}	// namespace czx