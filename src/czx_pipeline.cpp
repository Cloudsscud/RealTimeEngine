#include <czx_pipeline.h>
#include <czx_model.h>

// std
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <cassert>

namespace czx {

	CzxPipeline::CzxPipeline(
		CzxDevice& device,
		const std::string& vertFilePath,
		const std::string& fragFilePath,
		const PipelineConfigInfo& configInfo)
		: m_device(device) {

		createGraphicsPipeline(vertFilePath, fragFilePath, configInfo);
	}

	CzxPipeline::~CzxPipeline() {
		vkDestroyShaderModule(m_device.device(), m_vertShaderModule, nullptr);
		vkDestroyShaderModule(m_device.device(), m_fragShaderModule, nullptr);
		vkDestroyPipeline(m_device.device(), m_graphicsPipeline, nullptr);
	}

	std::vector<char> CzxPipeline::readFile(const std::string& filePath) {

		std::ifstream file{ filePath, std::ios::ate | std::ios::binary };	// ate 文件打开时自动跳转文件末尾，方便得到文件大小 
																			// binary 二进制读取防止文本转换
		if (!file.is_open()) {
			throw std::runtime_error("failed to open file: " + filePath);
		}

		size_t fileSize = static_cast<size_t>(file.tellg());	// 由于指针已经在文件末尾，所以tellg就得到的是文件大小(int)

		std::vector<char> buffer(fileSize);

		file.seekg(0);	// 到文件开头
		file.read(buffer.data(), fileSize);	// 读到buffer中

		file.close();
		return buffer;
	}

	void CzxPipeline::createGraphicsPipeline(
		const std::string& vertFilePath,
		const std::string& fragFilePath,
		const PipelineConfigInfo& configInfo) {

		assert(
			configInfo.pipelineLayout != VK_NULL_HANDLE &&
			"Cannot create graphics pipeline:: no pipelineLayout provided in configInfo");
		assert(
			configInfo.renderPass != VK_NULL_HANDLE &&
			"Cannot create graphics pipeline:: no renderPass provided in configInfo");

		// 读取shader
		auto vertCode = readFile(vertFilePath);
		auto fragCode = readFile(fragFilePath);

		// 绑定着色器代码与模块
		createShaderModule(vertCode, &m_vertShaderModule);
		createShaderModule(fragCode, &m_fragShaderModule);

		// 配置管线着色器阶段设置
		VkPipelineShaderStageCreateInfo shaderStages[2];
		shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;		// 着色器类型
		shaderStages[0].module = m_vertShaderModule;			// 对应的着色器模块
		shaderStages[0].pName = "main";							// 入口函数名，shader内部调用的入口函数名
		shaderStages[0].flags = 0;								// 扩展标志
		shaderStages[0].pNext = nullptr;						// 扩展链指针
		shaderStages[0].pSpecializationInfo = nullptr;			// 着色器特化常量，用于编译器优化 (非空可在管线创建时动态修改着色器常量，而不用重新编译着色器)
		shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		shaderStages[1].module = m_fragShaderModule;
		shaderStages[1].pName = "main";
		shaderStages[1].flags = 0;
		shaderStages[1].pNext = nullptr;
		shaderStages[1].pSpecializationInfo = nullptr;

		// 配置管线顶点输入设置
		// 根据定义好的vertex buffer来完善管线的定点输入配置
		auto bindingDescriptions = CzxModel::Vertex::getBindingDescriptions();
		auto attributeDescriptions = CzxModel::Vertex::getAttributeDescriptions();

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
		vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();

		// 创建图形管线
		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;		// 顶点输入
		pipelineInfo.pInputAssemblyState = &configInfo.inputAssemblyInfo;	// 拓扑
		pipelineInfo.pViewportState = &configInfo.viewportInfo;				// 视口与裁剪
		pipelineInfo.pRasterizationState = &configInfo.rasterizationInfo;	// 光栅化
		pipelineInfo.pMultisampleState = &configInfo.multisampleInfo;		// 多重采样
		pipelineInfo.pColorBlendState = &configInfo.colorBlendInfo;			// 颜色混合
		pipelineInfo.pDepthStencilState = &configInfo.depthStencilInfo;		// optional深度模板	
		pipelineInfo.pDynamicState = &configInfo.dynamicStateInfo;								// optional动态状态;nullptr所有状态创建时固定

		pipelineInfo.layout = configInfo.pipelineLayout;
		pipelineInfo.renderPass = configInfo.renderPass;
		pipelineInfo.subpass = configInfo.subpass;

		// 优化点1 管线继承:多管线创建优化
		pipelineInfo.basePipelineIndex = -1;		// 所依赖管线在pPipelines的下标
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;	// 已有管线的句柄，从该管线继承状态

		// 优化点2 管线缓存：VK_NULL_HANDLE不缓存，每次从头编译管线
		if (vkCreateGraphicsPipelines(m_device.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS) {
			throw std::runtime_error("failed to create graphics pipeline");
		}
	}

	void CzxPipeline::createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule) {
		// 创建着色器模块时需要一个指向着色器模块的创建信息结构体的指针
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		if (vkCreateShaderModule(m_device.device(), &createInfo, nullptr, shaderModule) != VK_SUCCESS) {
			throw std::runtime_error("failed to create shader module");
		}
	}

	void CzxPipeline::bind(VkCommandBuffer commandBuffer) {
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
	}


	void CzxPipeline::defaultPipelineConfigInfo(PipelineConfigInfo& configInfo) {
		// 输入装配设置
		configInfo.inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;	// 自描述，记录类型
		configInfo.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;	// 规定装配图元的拓扑结构为每三点一独立三角形
		// POINT_LIST点列表，LINE_LIST两点一独立线段，TRIANGLE_STRIP三角形条带：每点与前两点一个共边三角形...
		configInfo.inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;	// 禁用图元重启；仅STRIP条带拓扑时为VK_TRUE来通过插入特殊索引0xFFFF/0xFFFFFF截断条带，而不用重绑定顶点缓冲区

		configInfo.viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		configInfo.viewportInfo.viewportCount = 1;						// 单视口
		configInfo.viewportInfo.pViewports = nullptr;
		configInfo.viewportInfo.scissorCount = 1;						// 单裁剪矩形
		configInfo.viewportInfo.pScissors = nullptr;

		// 配置光栅化设置
		configInfo.rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		configInfo.rasterizationInfo.depthClampEnable = VK_FALSE;			// 禁用深度截断，禁止深度被截断为[0,1]
		configInfo.rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;	// 禁止完全跳过光栅化阶段，若跳过则只执行顶点着色器
		configInfo.rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;	// 控制三角形填充方式，fill填充，line仅线框，point仅顶点
		configInfo.rasterizationInfo.lineWidth = 1.0f;						// 线宽，规范规定为1.0f，拓展后可其他大小
		configInfo.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;			// 面剔除，剔除/不渲染特定朝向的面,none不剔除,front_bit正面，back_bit背面(内部)
		configInfo.rasterizationInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;	// 规定正面为顶点顺时针排列的面
		configInfo.rasterizationInfo.depthBiasEnable = VK_FALSE;			// 禁用深度偏移，深度偏移用于解决深度冲突，防止闪烁；shadowmap时启用避免阴影痤疮
		configInfo.rasterizationInfo.depthBiasConstantFactor = 0.0f;		// 固定偏移量，加在片元深度值
		configInfo.rasterizationInfo.depthBiasClamp = 0.0f;					// 偏移量最大值
		configInfo.rasterizationInfo.depthBiasSlopeFactor = 0.0f;			// 根据三角形斜率动态调整偏移量

		// 配置多重采样设置
		configInfo.multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		configInfo.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;	// 每像素采样点数，设为1表示关闭抗锯齿
		configInfo.multisampleInfo.sampleShadingEnable = VK_FALSE;		// 禁用采样着色，单像素内所有采样点用相同的片元着色器结果；启用后采样点都执行片元着色器来提升抗锯齿质量
		configInfo.multisampleInfo.minSampleShading = 1.0f;				// 片元着色器至少覆盖多少比例的采样点
		configInfo.multisampleInfo.pSampleMask = nullptr;				// 采样掩码数组指针，精细控制哪些采样点被启用;nullptr全启用
		configInfo.multisampleInfo.alphaToCoverageEnable = VK_FALSE;	// 用于透明纹理抗锯齿
		configInfo.multisampleInfo.alphaToOneEnable = VK_FALSE;			// alpha强制置为1，完全不透明，禁用

		// 配置颜色混合附件与颜色混合设置，控制像素写入帧缓冲
		configInfo.colorBlendAttachment.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT; // 颜色写入掩码，控制写入帧缓冲的颜色通道，启用RGBA四通道
		configInfo.colorBlendAttachment.blendEnable = VK_FALSE;						// 禁用颜色混合，新像素覆盖旧像素，默认配置适用于不透明物体
		configInfo.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;	// 颜色混合RGB执行操作:1*src+0*dst
		configInfo.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		configInfo.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		configInfo.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;	// 颜色混合透明度alpha执行操作:1*src+0*dst
		configInfo.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		configInfo.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

		configInfo.colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		configInfo.colorBlendInfo.logicOpEnable = VK_FALSE;		// 禁用逻辑操作的混合方式（基于位运算）
		configInfo.colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;	// 逻辑操作中的复制源颜色到目标
		configInfo.colorBlendInfo.attachmentCount = 1;			// 一个颜色附件，单RenderTarget(framebuffer)(单相机)
		configInfo.colorBlendInfo.pAttachments = &configInfo.colorBlendAttachment;
		configInfo.colorBlendInfo.blendConstants[0] = 0.0f;		// 四个混合常量
		configInfo.colorBlendInfo.blendConstants[1] = 0.0f;
		configInfo.colorBlendInfo.blendConstants[2] = 0.0f;
		configInfo.colorBlendInfo.blendConstants[3] = 0.0f;

		// 配置深度模板，depth buffer
		configInfo.depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		configInfo.depthStencilInfo.depthTestEnable = VK_TRUE;			// 启用深度测试:当前片元的深度与depth buffer存的比较来决定是否保留片元；禁用新绘制的直接覆盖旧的
		configInfo.depthStencilInfo.depthWriteEnable = VK_TRUE;			// 允许更新depth buffer的值；透明物体不建议更新
		configInfo.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;// 定义新片元depth与buffer比较的操作符，新 < buffer允许绘制
		configInfo.depthStencilInfo.depthBoundsTestEnable = VK_FALSE;	// 用于规定允许的深度范围，false所有片元均通过，true则只找深度在范围内的片元
		configInfo.depthStencilInfo.minDepthBounds = 0.0f;
		configInfo.depthStencilInfo.maxDepthBounds = 1.0f;
		configInfo.depthStencilInfo.stencilTestEnable = VK_FALSE;		// 模板测试，使用模板缓冲来控制片元的写入权限
		configInfo.depthStencilInfo.front = {};							// 正面模板测试参数
		configInfo.depthStencilInfo.back = {};							// 背面模板测试参数

		configInfo.dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		configInfo.dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		configInfo.dynamicStateInfo.pDynamicStates = configInfo.dynamicStateEnables.data();
		configInfo.dynamicStateInfo.dynamicStateCount = configInfo.dynamicStateEnables.size();
		configInfo.dynamicStateInfo.flags = 0;

	}

}