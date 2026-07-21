#include <czx_pipeline.h>

// std
#include <fstream>
#include <stdexcept>
#include <iostream>

namespace czx {

	CzxPipeline::CzxPipeline(const std::string& vertFilePath, const std::string& fragFilePath) {
		CreateGraphicsPipeline(vertFilePath, fragFilePath);
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

	void CzxPipeline::CreateGraphicsPipeline(
		const std::string& vertFilePath, const std::string& fragFilePath) {

		auto vertCode = readFile(vertFilePath);
		auto fragCode = readFile(fragFilePath);

		std::cout << "Vertex Shader Code Size: " << vertCode.size() << '\n';
		std::cout << "fragment Shader Code Size: " << fragCode.size() << '\n';
	}


}