#pragma once

#include <czx_device.h>

// std
#include <string>
#include <vector>

namespace czx {

	// 管线配置信息，方便应用层快速管理管线，允许多个管线复用相同配置，管线创建后通常不再修改。
	struct PipelineConfigInfo{
		PipelineConfigInfo() = default;
		PipelineConfigInfo(const PipelineConfigInfo&) = delete;
		PipelineConfigInfo& operator=(const PipelineConfigInfo&) = delete;

		VkPipelineViewportStateCreateInfo viewportInfo{};  // 描述视口和裁剪矩形范围，决定渲染输出区域
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};  // 用于配置输入装配阶段，把顶点数据组装成图元，创建后拓扑通常固定
		VkPipelineRasterizationStateCreateInfo rasterizationInfo{};  // 用于控制几何体如何被光栅化为片元，决定是否开启背面剔除等特性
		VkPipelineMultisampleStateCreateInfo multisampleInfo{};  // 控制多重采样抗锯齿等采样方式
		VkPipelineColorBlendAttachmentState colorBlendAttachment{};  // 控制单个颜色附件的混合公式和写入掩码
		VkPipelineColorBlendStateCreateInfo colorBlendStateInfo{};  // 封装所有颜色附件的混合状态，使用全局混合常量
		VkPipelineDepthStencilStateCreateInfo depthStencilStateInfo{};  // 控制深度测试、深度写入、比较操作以及模板测试
		std::vector<VkDynamicState> dynamicStateEnables{};  // 允许在管线创建后动态修改的状态列表，如视口和裁剪矩形
		VkPipelineDynamicStateCreateInfo dynamicStateInfo{};  // 描述动态状态的配置结构体
		// 默认管线配置不处理管线布局与渲染通道，在函数外设置
		VkPipelineLayout pipelineLayout = nullptr;  // 描述资源绑定布局，告诉管线如何访问着色器资源
		VkRenderPass renderPass = nullptr;  // 描述帧缓冲的格式、采样数和加载存储操作
		uint32_t subpass = 0;  // 指定渲染通道中的子通道索引
	};

	// 负责从SPIR-V着色器文件中创建并管理Vulkan图形管线，封装着色器模块和渲染状态配置
	class CzxPipeline {
	public:
		CzxPipeline(
			CzxDevice& device,
			const std::string& vertFilePath,
			const std::string& fragFilePath,
			const PipelineConfigInfo& configInfo);  // 根据着色器路径和配置创建图形管线。

		~CzxPipeline();  // 销毁着色器模块和图形管线对象。

		// 禁用复制
		CzxPipeline(const CzxPipeline&) = delete;  // 禁止拷贝，避免多个对象同时持有同一管线资源。
		CzxPipeline& operator=(const CzxPipeline&) = delete;  // 禁止赋值，防止重复清理相同句柄。

		void bind(VkCommandBuffer commandBuffer);  // 将当前管线绑定到命令缓冲区，后续绘制会使用这条管线。

		static void defaultPipelineConfigInfo(PipelineConfigInfo& configInfo);  // 提供一组默认的管线配置，供调用方快速初始化。

	private:
		static std::vector<char> readFile(const std::string& filePath);  // 从磁盘读取SPIR-V着色器文件内容。

		void createGraphicsPipeline(
			const std::string& vertFilePath,
			const std::string& fragFilePath,
			const PipelineConfigInfo& configInfo);

		void createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule);  // 将着色器字节码包装成Vulkan着色器模块

		CzxDevice& m_device;
		VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;  // Vulkan图形管线句柄
		VkShaderModule m_vertShaderModule = VK_NULL_HANDLE;  // 顶点着色器模块句柄
		VkShaderModule m_fragShaderModule = VK_NULL_HANDLE;  // 片段着色器模块句柄
	};
}	// namespace czx