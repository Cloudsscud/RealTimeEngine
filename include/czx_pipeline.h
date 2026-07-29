#pragma once

#include <czx_device.h>

// std
#include <string>
#include <vector>

namespace czx {

	// 管线配置信息，方便应用层快速管理管线，允许多管线使用相同配置，创建管线后固定
	struct PipelineConfigInfo{
		VkViewport viewport;
		VkRect2D scissor;
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;	// 用于配置输入装配阶段的结构体，将输入数据组装为图元，创建后固定拓扑，不同拓扑需多管线
		VkPipelineRasterizationStateCreateInfo rasterizationInfo;	// 用于控制将多边形转换为片元，创建后固定，不同片元类型需多管线
		VkPipelineMultisampleStateCreateInfo multisampleInfo;		// 控制多重采样抗锯齿MSAA
		VkPipelineColorBlendAttachmentState colorBlendAttachment;	// 控制单帧缓冲附件(RenderTarget)的混合公式
		VkPipelineColorBlendStateCreateInfo colorBlendInfo;			// 封装附件，使用全局混合常量
		VkPipelineDepthStencilStateCreateInfo depthStencilInfo;		// 控制深度测试、深度写入、比较操作符、模板测试，创建后固定，不同深度测试需多管线
		// 默认管线配置不处理管线布局与渲染通道，在函数外设置
		VkPipelineLayout pipelineLayout = nullptr;					// 描述资源绑定布局
		VkRenderPass renderPass = nullptr;							// 描述帧缓冲的格式、采样数、加载存储操作
		uint32_t subpass = 0;										// 对应RenderPass某个子通道
	};

	class CzxPipeline {
	public:
		CzxPipeline(
			CzxDevice& device,
			const std::string& vertFilePath,
			const std::string& fragFilePath,
			const PipelineConfigInfo& configInfo);

		~CzxPipeline();

		// 禁用复制
		CzxPipeline(const CzxPipeline&) = delete;
		CzxPipeline& operator=(const CzxPipeline&) = delete;

		void bind(VkCommandBuffer commandBuffer);

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