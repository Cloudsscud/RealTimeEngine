#pragma once

#include <czx_device.h>
#include <czx_buffer.h>

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

// std
#include <string>
#include <memory>
#include <vector>

namespace czx {

    class CzxTexture {
    public:
        /**
         * @brief 从文件读取图像创建纹理/贴图
         * @param device 逻辑设备
         * @param filePath 纹理/贴图文件路径
         * @param format default=RGBA8_SRGB 色彩校正后图像格式
         */
        CzxTexture(
            CzxDevice& device,
            const std::string& filePath,
            VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
        /**
         * @brief 占位贴图
         * @param device 逻辑设备
         * @param format default=RGBA8_UNORM 数值图像格式
         */
        CzxTexture(
            CzxDevice& device,
            VkFormat format = VK_FORMAT_R8G8B8A8_UNORM);

        ~CzxTexture();

        CzxTexture(const CzxTexture&) = delete;
        CzxTexture& operator=(const CzxTexture&) = delete;
        CzxTexture(CzxTexture&&) = default;
        CzxTexture& operator=(CzxTexture&&) = default;

        VkDescriptorImageInfo getDescriptorInfo() const;

        // 从文件加载纹理的静态方法
        static std::unique_ptr<CzxTexture> createTextureFromFile(
            CzxDevice& device,
            const std::string& filePath);

    private:
        void createImageFromFile(const std::string& filePath, VkFormat format);
        void createImageView(VkFormat format);
        void createSampler();

        int getBytesPerTexFormat(VkFormat format);

        CzxDevice& m_device;

    public:
        VkImage m_image = VK_NULL_HANDLE;
        VkDeviceMemory m_imageMemory = VK_NULL_HANDLE;
        VkImageView m_imageView = VK_NULL_HANDLE;
        VkSampler m_sampler = VK_NULL_HANDLE;

        uint32_t m_width = 0;
        uint32_t m_height = 0;
        uint32_t m_mipLevels = 1;
    };

} // namespace czx