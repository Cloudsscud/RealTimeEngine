#include <czx_pipeline.h>

// std
#include <fstream>
#include <stdexcept>
#include <iostream>

namespace czx {

	CzxPipeline::CzxPipeline(
		CzxDevice& device,
		const std::string& vertFilePath,
		const std::string& fragFilePath,
		const PipelineConfigInfo& configInfo)
		: m_device(device) {

		createGraphicsPipeline(vertFilePath, fragFilePath, configInfo);
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

		auto vertCode = readFile(vertFilePath);
		auto fragCode = readFile(fragFilePath);

		std::cout << "Vertex Shader Code Size: " << vertCode.size() << '\n';
		std::cout << "fragment Shader Code Size: " << fragCode.size() << '\n';
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

	PipelineConfigInfo CzxPipeline::defaultPipelineConfigInfo(uint32_t width, uint32_t height) {
		PipelineConfigInfo configInfo{};

		return configInfo;
	}

}