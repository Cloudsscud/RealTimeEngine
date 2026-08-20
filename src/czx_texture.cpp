#include <czx_texture.h>
#include <czx_utils.h>

// libs
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

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
        createImageFromFile(filePath, format);
        createImageView(format);
        createSampler();
        printf("Texture from %s created\n", filePath.c_str());
    }

    CzxTexture::CzxTexture(
        CzxDevice& device,
        VkFormat format)
        : m_device(device) {
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

    // 当前格式的每像素字节数
    int CzxTexture::getBytesPerTexFormat(VkFormat format) {
        switch (format) {
        case VK_FORMAT_R8_SINT:
        case VK_FORMAT_R8_UNORM:
            return 1;
        case VK_FORMAT_R16_SFLOAT:
            return 2;
        case VK_FORMAT_R16G16_SFLOAT:
            return 4;
        case VK_FORMAT_R16G16_SNORM:
            return 4;
        case VK_FORMAT_R8G8B8A8_UNORM:
            return 4;
        case VK_FORMAT_R8G8B8A8_SRGB:
            return 4;
        case VK_FORMAT_B8G8R8A8_UNORM:
            return 4;
        case VK_FORMAT_R16G16B16_SFLOAT:
            return 4 * sizeof(uint16_t);
        case VK_FORMAT_R32G32B32A32_SFLOAT:
            return 4 * sizeof(float);
        default:
            ERROR("Unknown format %d\n", format);
        }
        return 0;
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

    void CzxTexture::createImageFromFile(const std::string& filePath, VkFormat format) {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(filePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

        if (!pixels) {
            throw std::runtime_error("Failed to load texture image: " + filePath);
        }

        int BytesPerPixel = getBytesPerTexFormat(format);
        m_width = static_cast<uint32_t>(texWidth);
        m_height = static_cast<uint32_t>(texHeight);
        VkDeviceSize LayerSize = m_width * m_height * BytesPerPixel;    // 每层大小

        int LayerCount = 1;
        VkDeviceSize imageSize = LayerSize * LayerCount;

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

        m_device.createImage(
            m_width,m_height,format,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            m_image,
            m_imageMemory
        );

        // 转换图像布局并拷贝数据  UNDEFINED -> TRANSFER_DST_OPTIMAL -> copy -> SHADER_READ_ONLY_OPTIMAL(fragment shader访问)
        m_device.transitionImageLayout(m_image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        m_device.copyBufferToImage(stageBuffer.getBuffer(), m_image, m_width, m_height, 1);
        m_device.transitionImageLayout(m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    void CzxTexture::createImageView(VkFormat format) {
        m_imageView = m_device.createImageView(
            m_image,
            format,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_VIEW_TYPE_2D,
            1,
            m_mipLevels
        );
    }

    void CzxTexture::createSampler() {
        m_sampler = m_device.createTextureSampler();
    }


} // namespace czx