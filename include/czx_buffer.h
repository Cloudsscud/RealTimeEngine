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

        /**
         * @brief 将缓冲区的指定内存范围映射到主机虚拟地址空间
         * @param size default=VK_WHOLE_SIZE 表示映射整个缓冲区 要映射的内存大小
         * @param offset default=0 从缓冲区起始位置的字节偏移
         * @return VkResult 映射操作的结果码
         * @note 映射成功后，可通过 m_mapped 指针访问缓冲区内容
         */
        VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

        /**
         * @brief 解除缓冲区的主机内存映射
         */
        void unmap();

        /**
           * @brief 将主机数据拷贝到已映射的缓冲区中
           * @param data 指向源数据的指针
           * @param size default=VK_WHOLE_SIZE 表示拷贝整个缓冲区 要拷贝的字节数
           * @param offset default=0 从缓冲区起始位置的字节偏移
           * @note 调用前必须已通过 map() 映射缓冲区
           * @warning 如果 offset + size 超出缓冲区范围，将导致未定义行为
           */
        void writeToBuffer(void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        VkDescriptorBufferInfo descriptorInfo(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

        /**
         * @brief 使缓冲区内存范围失效，让主机读取设备写入的最新数据
         * @param size default=VK_WHOLE_SIZE 要失效的内存大小
         * @param offset default=0 从缓冲区起始位置的字节偏移
         * @return VkResult 失效操作的结果码
         * @note 仅对非一致性内存（non-coherent memory）需要此操作
         */
        VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);

        // 按索引写入单个实例数据
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
        /**
         * @brief 计算满足最小对齐偏移要求的实例对齐大小
         * @param instanceSize 单个实例的原始字节大小
         * @param minOffsetAlignment 最小对齐偏移字节数（必须是2的幂）
         * @return 对齐后的实例大小（向上取整到 minOffsetAlignment 的整数倍）
         */
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