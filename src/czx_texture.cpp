#include <czx_texture.h>
#include <czx_utils.h>

// libs
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// std
#include <stdexcept>
#include <iostream>
#include <cstring>

namespace czx {

    CzxTexture::CzxTexture(
        CzxDevice& device,
        const std::string& filePath,
        VkFormat format)
        : m_device(device) {
        createImage(filePath, format);
        createImageView(format);
        createSampler();
    }

    CzxTexture::~CzxTexture() {
        if (m_sampler != VK_NULL_HANDLE) {
            vkDestroySampler(m_device.device(), m_sampler, nullptr);
        }
        if (m_imageView != VK_NULL_HANDLE) {
            vkDestroyImageView(m_device.device(), m_imageView, nullptr);
        }
        if (m_image != VK_NULL_HANDLE) {
            vkDestroyImage(m_device.device(), m_image, nullptr);
        }
        if (m_imageMemory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device.device(), m_imageMemory, nullptr);
        }
    }

    VkDescriptorImageInfo CzxTexture::getDescriptorInfo() const {
        return VkDescriptorImageInfo{
            m_sampler,
            m_imageView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
    }

    std::unique_ptr<CzxTexture> CzxTexture::createTextureFromFile(
        CzxDevice& device,
        const std::string& filePath) {
        return std::make_unique<CzxTexture>(device, filePath);
    }

    void CzxTexture::createImage(const std::string& filePath, VkFormat format) {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(filePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

        if (!pixels) {
            throw std::runtime_error("Failed to load texture image: " + filePath);
        }

        m_width = static_cast<uint32_t>(texWidth);
        m_height = static_cast<uint32_t>(texHeight);
        VkDeviceSize imageSize = m_width * m_height * 4;

        std::cout << "Loaded texture: " << filePath << std::endl;
        std::cout << "  Width: " << m_width << ", Height: " << m_height << std::endl;

        // 创建暂存缓冲区，用于上传像素数据
        CzxBuffer stageBuffer(
            m_device,
            imageSize,
            1,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        stageBuffer.map();
        stageBuffer.writeToBuffer(pixels);
        stageBuffer.unmap();

        // 释放CPU端像素数据
        stbi_image_free(pixels);

        // 创建图像
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = m_width;
        imageInfo.extent.height = m_height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = m_mipLevels;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        m_device.createImageWithInfo(
            imageInfo,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_image,
            m_imageMemory
        );

        // 转换图像布局并拷贝数据  UNDEFINED -> TRANSFER_DST -> copy -> SHADER_READ_ONLY
        m_device.transitionImageLayout(m_image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        m_device.copyBufferToImage(stageBuffer.getBuffer(), m_image, m_width, m_height, 1);
        m_device.transitionImageLayout(m_image, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    void CzxTexture::createImageView(VkFormat format) {
        m_imageView = m_device.createImageView(
            m_image,
            format,
            VK_IMAGE_ASPECT_COLOR_BIT,
            m_mipLevels,
            0,
            1,
            0,
            VK_IMAGE_VIEW_TYPE_2D
        );
    }

    void CzxTexture::createSampler() {
        m_sampler = m_device.createDefaultSampler();
    }


} // namespace czx