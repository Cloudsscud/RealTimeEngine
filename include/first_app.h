#pragma once

#include <czx_window.h>
#include <czx_pipeline.h>

namespace czx {
	class FirstAPP {
	public:
		static constexpr int WIDTH = 800;
		static constexpr int HEIGHT = 600;

		void run();
	private:
		CzxWindow m_window{WIDTH, HEIGHT, "Hello Vulkan!"};
		CzxPipeline m_pipeline{ "shaders/simple_shader.vert.spv", "shaders/simple_shader.frag.spv" };
	};
}	// namespace czx