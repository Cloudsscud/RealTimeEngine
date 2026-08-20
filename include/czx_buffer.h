#pragma once

#include "czx_device.h"

namespace czx {

    class CzxBuffer {
    public:

        /**
         * @brief 缓冲区的封装结构
         * @param device 逻辑设备
         * @param instanceSize 单个实例占用的字节大小
         * @param instanceCount 缓冲区包含的实例数量
         * @param usage 缓冲区的用途
         * @param memoryProps 缓冲区内存应具备的属性，例如HOST_VISIBLE或DEVICE_LOCAL
         * @param minOffsetAlignment default=1 最小内存对齐偏移字节数
         */
        CzxBuffer(
            CzxDevice& device,
            VkDeviceSize instanceSize,
            uint32_t instanceCount,
            VkBufferUsageFlags usage,
            VkMemoryPropertyFlags memoryProps,
            VkDeviceSize minOffsetAlignment = 1);
        ~CzxBuffer();

        CzxBuffer(const CzxBuffer&) = delete;
        CzxBuffer& operator=(const CzxBuffer&) = delete;

        VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        void unmap();

        void writeToBuffer(void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        VkDescriptorBufferInfo descriptorInfo(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

        void writeToIndex(void* data, int index);
        VkResult flushIndex(int index);
        VkDescriptorBufferInfo descriptorInfoForIndex(int index);
        VkResult invalidateIndex(int index);

        VkBuffer getBuffer() const { return m_buffer; }
        void* getMappedMemory() const { return m_mapped; }
        uint32_t getInstanceCount() const { return m_instanceCount; }
        VkDeviceSize getInstanceSize() const { return m_instanceSize; }
        VkDeviceSize getAlignmentSize() const { return m_instanceSize; }
        VkBufferUsageFlags getUsageFlags() const { return m_usage; }
        VkMemoryPropertyFlags getMemoryPropertyFlags() const { return m_memoryProps; }
        VkDeviceSize getBufferSize() const { return m_bufferSize; }

    private:
        static VkDeviceSize getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment);

        CzxDevice& m_device;
        void* m_mapped = nullptr;   // 虚拟地址，CPU写入数据(optional GPU可读)的地方
        VkBuffer m_buffer = VK_NULL_HANDLE;
        VkDeviceMemory m_bufferMemory = VK_NULL_HANDLE; // 实际内存

        VkDeviceSize m_bufferSize;  // 缓冲区总大小
        uint32_t m_instanceCount;   // 实例数
        VkDeviceSize m_instanceSize;    // 缓冲区内每个实例的大小
        VkDeviceSize m_alignmentSize;   // 计算后的实际偏移字节数
        VkBufferUsageFlags m_usage; // 当前缓冲区的用途
        VkMemoryPropertyFlags m_memoryProps;    // 当前缓冲区的内存特性
    };

}  // namespace czx