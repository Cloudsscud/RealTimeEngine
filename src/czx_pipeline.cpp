#include <czx_utils.h>
#include <czx_pipeline.h>
#include <czx_model.h>

// std
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <cassert>

namespace czx {

	// 构造函数接收设备对象和着色器文件路径，随后会创建出真正可用的图形管线对象。
	CzxPipeline::CzxPipeline(
		CzxDevice& device,  // 传入设备句柄，后续管线对象的创建和销毁都依赖它。
		const std::string& vertFilePath,  // 记录顶点着色器文件路径，后续从磁盘读取SPIR-V字节码。
		const std::string& fragFilePath,  // 记录片元着色器文件路径，后续从磁盘读取SPIR-V字节码。
		const PipelineConfigInfo& configInfo)  // 保存当前管线的渲染状态和布局配置，决定最终管线行为。
		: m_device(device) {  // 把设备引用存入成员变量，保证构造后的对象能够继续操作Vulkan设备。

		createGraphicsPipeline(vertFilePath, fragFilePath, configInfo);  // 调用核心创建逻辑，把顶点/片元着色器与渲染配置组合成完整管线。
	}

	// 析构函数负责释放管线占用的着色器模块和图形管线句柄，避免资源泄漏。
	CzxPipeline::~CzxPipeline() {  // 对象销毁时释放本次创建出来的Vulkan管线资源。
		vkDestroyShaderModule(m_device.device(), m_vertShaderModule, nullptr);  // 释放顶点着色器模块，避免驱动资源残留。
		vkDestroyShaderModule(m_device.device(), m_fragShaderModule, nullptr);  // 释放片元着色器模块，避免驱动资源残留。
		vkDestroyPipeline(m_device.device(), m_graphicsPipeline, nullptr);  // 释放图形管线对象，回收管线句柄占用的资源。
	}

	std::vector<char> CzxPipeline::readFile(const std::string& filePath) {

		std::ifstream file{ filePath, std::ios::ate | std::ios::binary }; 	// ate 文件打开时自动跳转文件末尾，方便得到文件大小 
																		// binary 二进制读取防止文本转换
		if (!file.is_open()) {  // 如果文件没有成功打开，就说明着色器路径非法或文件不存在。
			throw std::runtime_error("failed to open file: " + filePath);  // 抛出异常，明确提示读取失败的原因。
		}

		size_t fileSize = static_cast<size_t>(file.tellg());	// 由于指针已经在文件末尾，所以tellg就得到的是文件大小(int)

		std::vector<char> buffer(fileSize);  // 根据文件大小分配缓冲区，准备保存着色器字节码内容。

		file.seekg(0);	// 到文件开头  // 读取指针重新定位到文件开头，确保从头开始读取内容。
		file.read(buffer.data(), fileSize);	// 读到buffer中  // 把整个着色器文件内容读入缓冲区。

		file.close();  // 读取完成后关闭文件流，释放底层系统资源。
		return buffer;  // 返回保存着色器字节码的数组，后续可转换成ShaderModule。
	}

	void CzxPipeline::createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule) {
		VkShaderModuleCreateInfo createInfo{
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = code.size(),
			.pCode = reinterpret_cast<const uint32_t*>(code.data())
		};

		VkResult result = vkCreateShaderModule(m_device.device(), &createInfo, nullptr, shaderModule);
		CHECK_VK_RESULT(result, "create shader module\n");
	}

	// 组装着色器阶段、顶点输入状态和渲染状态，最终创建出完整的图形管线。
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

		// 读取shader内容，栈上存储自动销毁
		auto vertCode = readFile(vertFilePath);
		auto fragCode = readFile(fragFilePath);

		// 绑定着色器代码与模块
		createShaderModule(vertCode, &m_vertShaderModule);
		createShaderModule(fragCode, &m_fragShaderModule);

		// 配置管线着色器阶段设置
		VkPipelineShaderStageCreateInfo shaderStages[2] = {
			{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_VERTEX_BIT,		// 着色器类型
				.module = m_vertShaderModule,		// 对应的着色器模块
				.pName = "main",					// 入口函数名，shader内部调用的入口函数名
				.pSpecializationInfo = nullptr		// 着色器特化常量，用于编译器优化 (非空可在管线创建时动态修改着色器常量，而不用重新编译着色器)
			},
			{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
				.module = m_fragShaderModule,
				.pName = "main",
				.pSpecializationInfo = nullptr
			}
		};

		// 配置管线顶点输入设置
		// 根据定义好的vertex buffer来完善管线的顶点输入配置
		auto bindingDescriptions = CzxModel::Vertex::getBindingDescriptions();
		auto attributeDescriptions = CzxModel::Vertex::getAttributeDescriptions();

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size()),	// 绑定描述：输入缓冲存储数据的组织方式
			.pVertexBindingDescriptions = bindingDescriptions.data(),
			.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),	// 属性描述:一组属性的存储布局
			.pVertexAttributeDescriptions = attributeDescriptions.data()
		};


		// 创建图形管线
		VkGraphicsPipelineCreateInfo pipelineInfo{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = ARRAY_SIZE_IN_ELEMENTS(shaderStages),		// 使用到的所有着色器
			.pStages = &shaderStages[0],
			.pVertexInputState = &vertexInputInfo,					// 顶点输入
			.pInputAssemblyState = &configInfo.inputAssemblyInfo,	// 拓扑
			.pViewportState = &configInfo.viewportInfo,				// 视口与裁剪
			.pRasterizationState = &configInfo.rasterizationInfo,	// 光栅化
			.pMultisampleState = &configInfo.multisampleInfo,		// 多重采样
			.pDepthStencilState = &configInfo.depthStencilStateInfo,// optional深度模板	
			.pColorBlendState = &configInfo.colorBlendStateInfo,	// 颜色混合
			.pDynamicState = &configInfo.dynamicStateInfo,			// optional动态状态;nullptr所有状态创建时固定

			.layout = configInfo.pipelineLayout,		// 描述符集与推送常量信息
			.renderPass = configInfo.renderPass,		// 管线即将被使用的渲染通道
			.subpass = configInfo.subpass,				// 专用于渲染通道内的特定子通道

			// 优化点1 管线继承:多管线创建优化
			.basePipelineHandle = VK_NULL_HANDLE,	// 已有管线的句柄，从该管线继承状态
			.basePipelineIndex = -1,		// 所依赖管线在pPipelines的下标
		};

		// 优化点2 管线缓存：VK_NULL_HANDLE不缓存，每次从头编译管线
		VkResult result = vkCreateGraphicsPipelines(m_device.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline);
		CHECK_VK_RESULT(result, "create graphics pipeline\n");

		printf("Graphics pipeline created\n");
	}



	// 绑定当前管线到命令缓冲区，后续绘制调用会使用这条管线配置
	void CzxPipeline::bind(VkCommandBuffer commandBuffer) {
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);  // 通过命令缓冲区把当前管线设置为后续绘制的目标管线
	}


	// 默认管线配置
	void CzxPipeline::defaultPipelineConfigInfo(PipelineConfigInfo& configInfo) {  // 这个函数给管线配置一个通用且稳定的默认状态，后续可以在此基础上进行微调。
		// 输入装配设置
		configInfo.inputAssemblyInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,	// 规定装配图元的拓扑结构为每三点一独立三角形
			// POINT_LIST点列表，LINE_LIST两点一独立线段，TRIANGLE_STRIP三角形条带：每点与前两点一个共边三角形...
			.primitiveRestartEnable = VK_FALSE	// 禁用图元重启；仅STRIP条带拓扑时为VK_TRUE来通过插入特殊索引0xFFFF/0xFFFFFF截断条带，而不用重绑定顶点缓冲区
		};	// 图元装配阶段


		// 配置裁剪与视口信息，若窗口可变则不应在管线内设定：resize会导致管线重建，极大影响性能
		configInfo.viewportInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,	// 单视口  // 限定并转换图像在窗口的显示范围
			.pViewports = nullptr,
			.scissorCount = 1,	// 单裁剪矩形  // 当前只使用一个裁剪矩形，裁掉超出裁剪窗口范围的数据
			.pScissors = nullptr
		};


		// 配置光栅化设置
		configInfo.rasterizationInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.depthClampEnable = VK_FALSE,			// 禁用深度截断，禁止深度被截断为[0,1]
			.rasterizerDiscardEnable = VK_FALSE,	// 禁止完全跳过光栅化阶段，若跳过则只执行顶点着色器
			.polygonMode = VK_POLYGON_MODE_FILL,	// 控制三角形填充方式，fill填充，line仅线框，point仅顶点
			.cullMode = VK_CULL_MODE_NONE,			// 面剔除，剔除/不渲染特定朝向的面,none不剔除,front_bit正面，back_bit背面(内部)
			.frontFace = VK_FRONT_FACE_CLOCKWISE,	// 规定正面为顶点顺时针排列的面
			.depthBiasEnable = VK_FALSE,			// 禁用深度偏移，深度偏移用于解决深度冲突，防止闪烁；shadowmap时启用避免阴影痤疮
			.depthBiasConstantFactor = 0.0f,		// 固定偏移量，加在片元深度值
			.depthBiasClamp = 0.0f,					// 偏移量最大值
			.depthBiasSlopeFactor = 0.0f,			// 根据三角形斜率动态调整偏移量
			.lineWidth = 1.0f,						// 线宽，规范规定为1.0f，拓展后可其他大小
		};

		// 配置多重采样设置，默认不进行多重采样
		configInfo.multisampleInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,	// 每像素采样点数，设为1表示关闭抗锯齿
			.sampleShadingEnable = VK_FALSE,				// 禁用采样着色，单像素内所有采样点用相同的片元着色器结果；启用后采样点都执行片元着色器来提升抗锯齿质量
			.minSampleShading = 1.0f,						// 片元着色器至少覆盖多少比例的采样点
			.pSampleMask = nullptr,							// 采样掩码数组指针，精细控制哪些采样点被启用;nullptr全启用
			.alphaToCoverageEnable = VK_FALSE,				// 用于透明纹理抗锯齿
			.alphaToOneEnable = VK_FALSE					// alpha强制置为1，完全不透明，禁用
		};

		// 配置颜色混合附件与颜色混合配置信息，控制像素写入帧缓冲的方式
		configInfo.colorBlendAttachment = {
			.blendEnable = VK_FALSE,		// 禁用颜色混合，新像素覆盖旧像素，默认配置适用于不透明物体
			.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,			// 启用混合后：RGB颜色混合执行操作:1*src + 0*dst
			.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
			.colorBlendOp = VK_BLEND_OP_ADD,					// RGB混合的操作符

			.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,			// 启用混合后：alpha透明度混合执行操作:1*src+0*dst
			.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
			.alphaBlendOp = VK_BLEND_OP_ADD,					// 透明度混合的操作符
			.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT	// 颜色写入掩码，控制写入帧缓冲的颜色通道，启用RGBA四通道 
		};

		configInfo.colorBlendStateInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.logicOpEnable = VK_FALSE,		// 禁用逻辑操作的混合方式（基于位运算）
			.logicOp = VK_LOGIC_OP_COPY,	// 若启用：将逻辑操作中的复制源颜色到目标
			.attachmentCount = 1,			// 一个颜色附件，单RenderTarget(framebuffer)(单相机)
			.pAttachments = &configInfo.colorBlendAttachment,	// 使用的所有颜色混合附件
			.blendConstants = {0.0f,0.0f,0.0f,0.0f }			// 不使用额外常量混合
		};

		// 配置深度模板，depth buffer
		configInfo.depthStencilStateInfo = {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable = VK_TRUE,			// 启用深度测试:当前片元的深度与depth buffer存的比较来决定是否保留片元；禁用新绘制的直接覆盖旧的
			.depthWriteEnable = VK_TRUE,		// 允许更新depth buffer的值；透明物体不建议更新
			.depthCompareOp = VK_COMPARE_OP_LESS,// 定义新片元depth与buffer比较的操作符，新 < buffer允许绘制

			.depthBoundsTestEnable = VK_FALSE,	// 用于规定允许的深度范围，false所有片元均通过，true则只找深度在范围内的片元
			.stencilTestEnable = VK_FALSE,		// 模板测试，使用模板缓冲来控制片元的写入权限
			.front = {},			// 正面模板测试参数
			.back = {},				// 背面模板测试参数

			.minDepthBounds = 0.0f,  // 设置深度测试允许的最小边界值。
			.maxDepthBounds = 1.0f,  // 设置深度测试允许的最大边界值。
		};

		configInfo.dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };  // 设置动态状态集合，允许不重新创建管线情况下绘制时修改视口和裁剪矩形。
		configInfo.dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;  // 设置动态状态结构体类型。
		configInfo.dynamicStateInfo.pDynamicStates = configInfo.dynamicStateEnables.data();  // 把动态状态数组指针绑定到结构体中。
		configInfo.dynamicStateInfo.dynamicStateCount = configInfo.dynamicStateEnables.size();  // 设置动态状态数量为2。
		configInfo.dynamicStateInfo.flags = 0;  // 动态状态创建时没有额外标志。

		
	}

}