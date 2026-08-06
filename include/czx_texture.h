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

    // 纹理类，负责加载图片并创建Vulkan图像和采样器
    class CzxTexture {
    public:
        CzxTexture(
            CzxDevice& device,
            const std::string& filePath,
            VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);
        ~CzxTexture();

        CzxTexture(const CzxTexture&) = delete;
        CzxTexture& operator=(const CzxTexture&) = delete;
        CzxTexture(CzxTexture&&) = default;
        CzxTexture& operator=(CzxTexture&&) = default;

        // 获取图像视图，用于绑定到描述符集
        VkImageView getImageView() const { return m_imageView; }
        VkSampler getSampler() const { return m_sampler; }
        VkDescriptorImageInfo getDescriptorInfo() const;

        // 获取纹理尺寸
        uint32_t getWidth() const { return m_width; }
        uint32_t getHeight() const { return m_height; }

        // 从文件加载纹理的静态方法
        static std::unique_ptr<CzxTexture> createTextureFromFile(
            CzxDevice& device,
            const std::string& filePath);

    private:
        void createImage(const std::string& filePath, VkFormat format);
        void createImageView(VkFormat format);
        void createSampler();
        void transitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout);
        void copyBufferToImage(VkBuffer buffer);

        CzxDevice& m_device;

        VkImage m_image = VK_NULL_HANDLE;
        VkDeviceMemory m_imageMemory = VK_NULL_HANDLE;
        VkImageView m_imageView = VK_NULL_HANDLE;
        VkSampler m_sampler = VK_NULL_HANDLE;

        uint32_t m_width = 0;
        uint32_t m_height = 0;
        uint32_t m_mipLevels = 1;
    };

} // namespace czx