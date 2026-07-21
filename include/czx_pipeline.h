#pragma once

#include <czx_device.h>

// std
#include <string>
#include <vector>

namespace czx {

	// 管线配置信息，方便应用层快速管理管线，允许多管线使用相同配置
	struct PipelineConfigInfo{};

	class CzxPipeline {
	public:
		CzxPipeline(
			CzxDevice& device,
			const std::string& vertFilePath,
			const std::string& fragFilePath,
			const PipelineConfigInfo& configInfo);

		~CzxPipeline() {}

		// 禁用复制
		CzxPipeline(const CzxPipeline&) = delete;
		CzxPipeline& operator=(const CzxPipeline&) = delete;

		static PipelineConfigInfo defaultPipelineConfigInfo(uint32_t width, uint32_t height);

	private:
		static std::vector<char> readFile(const std::string& filePath);

		void createGraphicsPipeline(
			const std::string& vertFilePath,
			const std::string& fragFilePath,
			const PipelineConfigInfo& configInfo);

		void createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule);

		CzxDevice& m_device;	// 引用存储，管线依赖于设备，故成员变量device生命周期更长
		VkPipeline m_graphicsPipeline;	// vulkan图形管线的句柄
		VkShaderModule m_vertShaderModule;	// 顶点着色器的vk着色器模块
		VkShaderModule m_fragShaderModule;	// 片段着色器的vk着色器模块
	};
}	// namespace czx