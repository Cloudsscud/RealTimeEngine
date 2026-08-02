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

	// 从磁盘读取SPIR-V着色器字节码，后续可以把它包装成Vulkan着色器模块。
	std::vector<char> CzxPipeline::readFile(const std::string& filePath) {  // 这个函数负责把着色器文件内容读入内存，供后续创建ShaderModule使用。

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

	// 将着色器字节码封装为Vulkan的着色器模块，供管线使用。
	void CzxPipeline::createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule) {  // 这个函数把着色器字节码封装成Vulkan后续可直接使用的ShaderModule。
		// 创建着色器模块时需要一个指向着色器模块的创建信息结构体的指针
		VkShaderModuleCreateInfo createInfo{};  // 创建着色器模块创建信息结构体，描述要加载的着色器字节码。
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;  // 标记结构体类型为ShaderModule创建信息。
		createInfo.codeSize = code.size();  // 设置着色器字节码大小，Vulkan据此读取内容。
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());  // 把字节流转成Vulkan所需的uint32_t指针格式。

		if (vkCreateShaderModule(m_device.device(), &createInfo, nullptr, shaderModule) != VK_SUCCESS) {  // 调用Vulkan创建真正的ShaderModule对象。
			throw std::runtime_error("failed to create shader module");  // 创建失败时抛出异常，提示着色器模块构建有问题。
		}
	}

	// 绑定当前管线到命令缓冲区，后续绘制调用会使用这条管线配置。
	void CzxPipeline::bind(VkCommandBuffer commandBuffer) {  // 这个函数把当前图形管线绑定到命令缓冲区，后续绘制会使用这条管线。
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);  // 通过命令缓冲区把当前管线设置为后续绘制的目标管线。
	}


	// 提供一套默认的管线配置，涵盖输入装配、光栅化、混合与深度测试等常见状态。
	void CzxPipeline::defaultPipelineConfigInfo(PipelineConfigInfo& configInfo) {  // 这个函数给管线配置一个通用且稳定的默认状态，后续可以在此基础上进行微调。
		// 输入装配设置
		configInfo.inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;	// 自描述，记录类型  // 设置输入装配状态结构体类型，告诉Vulkan该配置用于图元装配阶段。
		configInfo.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;	// 规定装配图元的拓扑结构为每三点一独立三角形  // 当前场景使用三角形列表作为基础绘制拓扑。
		// POINT_LIST点列表，LINE_LIST两点一独立线段，TRIANGLE_STRIP三角形条带：每点与前两点一个共边三角形...
		configInfo.inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;	// 禁用图元重启；仅STRIP条带拓扑时为VK_TRUE来通过插入特殊索引0xFFFF/0xFFFFFF截断条带，而不用重绑定顶点缓冲区  // 当前不需要条带拓扑的特殊截断，因此关闭图元重启。

		configInfo.viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;  // 设置视口状态结构体类型，后续管线会用它定义渲染范围。
		configInfo.viewportInfo.viewportCount = 1;						// 单视口  // 当前只使用一个视口，覆盖整个交换链图像区域。
		configInfo.viewportInfo.pViewports = nullptr;  // 视口数组指针置空，表示使用默认视口配置。
		configInfo.viewportInfo.scissorCount = 1;						// 单裁剪矩形  // 当前只使用一个裁剪矩形，裁掉超出窗口范围的部分。
		configInfo.viewportInfo.pScissors = nullptr;  // 裁剪矩形数组指针置空，表示使用默认裁剪配置。

		// 配置光栅化设置
		configInfo.rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;  // 设置光栅化状态结构体类型。
		configInfo.rasterizationInfo.depthClampEnable = VK_FALSE;			// 禁用深度截断，禁止深度被截断为[0,1]  // 关闭深度截断，保留完整深度值供后续比较。
		configInfo.rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;	// 禁止完全跳过光栅化阶段，若跳过则只执行顶点着色器  // 保留正常光栅化流程，避免几何体被直接丢弃。
		configInfo.rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;	// 控制三角形填充方式，fill填充，line仅线框，point仅顶点  // 采用填充模式，绘制实心三角形。
		configInfo.rasterizationInfo.lineWidth = 1.0f;						// 线宽，规范规定为1.0f，拓展后可其他大小  // 设置线宽为1.0f，符合Vulkan默认要求。
		configInfo.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;			// 面剔除，剔除/不渲染特定朝向的面,none不剔除,front_bit正面，back_bit背面(内部)  // 当前关闭面剔除，保证所有三角形都会参与绘制。
		configInfo.rasterizationInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;	// 规定正面为顶点顺时针排列的面  // 这里把顺时针顶点顺序识别为正面，和当前顶点排列保持一致。
		configInfo.rasterizationInfo.depthBiasEnable = VK_FALSE;			// 禁用深度偏移，深度偏移用于解决深度冲突，防止闪烁；shadowmap时启用避免阴影痤疮  // 默认关闭深度偏移，避免额外修改深度值。
		configInfo.rasterizationInfo.depthBiasConstantFactor = 0.0f;		// 固定偏移量，加在片元深度值  // 设置偏移量为0，保持深度值不变。
		configInfo.rasterizationInfo.depthBiasClamp = 0.0f;					// 偏移量最大值  // 设置偏移上限为0。
		configInfo.rasterizationInfo.depthBiasSlopeFactor = 0.0f;			// 根据三角形斜率动态调整偏移量  // 设置斜率偏移系数为0，关闭动态深度偏移。

		// 配置多重采样设置
		configInfo.multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;  // 设置多重采样状态结构体类型。
		configInfo.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;	// 每像素采样点数，设为1表示关闭抗锯齿  // 当前使用单采样，关闭MSAA以保持简单和稳定。
		configInfo.multisampleInfo.sampleShadingEnable = VK_FALSE;		// 禁用采样着色，单像素内所有采样点用相同的片元着色器结果；启用后采样点都执行片元着色器来提升抗锯齿质量  // 关闭采样着色，避免增加复杂度。
		configInfo.multisampleInfo.minSampleShading = 1.0f;				// 片元着色器至少覆盖多少比例的采样点  // 设置最小采样覆盖率为100%，让所有采样点都遵循同一结果。
		configInfo.multisampleInfo.pSampleMask = nullptr;				// 采样掩码数组指针，精细控制哪些采样点被启用;nullptr全启用  // 当前不使用自定义采样掩码，允许所有采样点生效。
		configInfo.multisampleInfo.alphaToCoverageEnable = VK_FALSE;	// 用于透明纹理抗锯齿  // 关闭alpha到覆盖率的抗锯齿路径，当前不需要特定透明效果。
		configInfo.multisampleInfo.alphaToOneEnable = VK_FALSE;			// alpha强制置为1，完全不透明，禁用  // 关闭alpha强制置1，保持正常透明度处理。

		// 配置颜色混合附件与颜色混合设置，控制像素写入帧缓冲
		configInfo.colorBlendAttachment.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT; // 颜色写入掩码，控制写入帧缓冲的颜色通道，启用RGBA四通道  // 允许把R、G、B、A四个通道都写入帧缓冲。
		configInfo.colorBlendAttachment.blendEnable = VK_FALSE;						// 禁用颜色混合，新像素覆盖旧像素，默认配置适用于不透明物体  // 当前不启用混合，直接覆盖旧像素。
		configInfo.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;	// 颜色混合RGB执行操作:1*src+0*dst  // 设置混合系数为1，保证当前颜色完全覆盖目标颜色。
		configInfo.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		configInfo.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		configInfo.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;	// 颜色混合透明度alpha执行操作:1*src+0*dst  // 设置透明通道混合系数为1，保持不透明覆盖效果。
		configInfo.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		configInfo.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

		configInfo.colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;  // 设置颜色混合状态结构体类型。
		configInfo.colorBlendInfo.logicOpEnable = VK_FALSE;		// 禁用逻辑操作的混合方式（基于位运算）  // 当前不使用逻辑操作混合，避免复杂的像素位运算。
		configInfo.colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;	// 逻辑操作中的复制源颜色到目标  // 逻辑操作为复制，保持颜色值直接写入目标。
		configInfo.colorBlendInfo.attachmentCount = 1;			// 一个颜色附件，单RenderTarget(framebuffer)(单相机)  // 当前场景只存在一个颜色附件，也就是单个帧缓冲目标。
		configInfo.colorBlendInfo.pAttachments = &configInfo.colorBlendAttachment;  // 把颜色混合附件配置绑定到颜色混合状态对象上。
		configInfo.colorBlendInfo.blendConstants[0] = 0.0f;		// 四个混合常量  // 设置混合常量的四个分量为0，表示不使用额外常量混合。
		configInfo.colorBlendInfo.blendConstants[1] = 0.0f;
		configInfo.colorBlendInfo.blendConstants[2] = 0.0f;
		configInfo.colorBlendInfo.blendConstants[3] = 0.0f;

		// 配置深度模板，depth buffer
		configInfo.depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;  // 设置深度模板状态结构体类型。
		configInfo.depthStencilInfo.depthTestEnable = VK_TRUE;			// 启用深度测试:当前片元的深度与depth buffer存的比较来决定是否保留片元；禁用新绘制的直接覆盖旧的  // 开启深度测试后，只有更靠近摄像机的片元才会被保留下来。
		configInfo.depthStencilInfo.depthWriteEnable = VK_TRUE;			// 允许更新depth buffer的值；透明物体不建议更新  // 开启深度写入，后续片元会更新深度缓冲内容。
		configInfo.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;// 定义新片元depth与buffer比较的操作符，新 < buffer允许绘制  // 使用小于比较，保证更近的物体覆盖更远的物体。
		configInfo.depthStencilInfo.depthBoundsTestEnable = VK_FALSE;	// 用于规定允许的深度范围，false所有片元均通过，true则只找深度在范围内的片元  // 关闭深度范围裁剪，所有片元都参与比较。
		configInfo.depthStencilInfo.minDepthBounds = 0.0f;  // 设置深度测试允许的最小边界值。
		configInfo.depthStencilInfo.maxDepthBounds = 1.0f;  // 设置深度测试允许的最大边界值。
		configInfo.depthStencilInfo.stencilTestEnable = VK_FALSE;		// 模板测试，使用模板缓冲来控制片元的写入权限  // 关闭模板测试，当前场景不使用模板缓冲。
		configInfo.depthStencilInfo.front = {};							// 正面模板测试参数  // 正面模板测试参数留空，表示不启用模板测试。
		configInfo.depthStencilInfo.back = {};							// 背面模板测试参数  // 背面模板测试参数留空，表示不启用模板测试。

		configInfo.dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };  // 设置动态状态集合，允许运行时修改视口和裁剪矩形。
		configInfo.dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;  // 设置动态状态结构体类型。
		configInfo.dynamicStateInfo.pDynamicStates = configInfo.dynamicStateEnables.data();  // 把动态状态数组指针绑定到结构体中。
		configInfo.dynamicStateInfo.dynamicStateCount = configInfo.dynamicStateEnables.size();  // 设置动态状态数量为2。
		configInfo.dynamicStateInfo.flags = 0;  // 动态状态创建时没有额外标志。

	}

}