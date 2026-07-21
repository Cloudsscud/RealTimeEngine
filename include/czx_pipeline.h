#pragma once

// std
#include <string>
#include <vector>

namespace czx {
	class CzxPipeline {
	public:
		CzxPipeline(const std::string& vertFilePath, const std::string& fragFilePath);

	private:
		static std::vector<char> readFile(const std::string& filePath);

		void CreateGraphicsPipeline(const std::string& vertFilePath, const std::string& fragFilePath);
	};
}	// namespace czx