#include "czx_buffer.h"

 // std
#include <cassert>
#include <cstring>

namespace czx {

    VkDeviceSize CzxBuffer::getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment) {
        assert((minOffsetAlignment & (minOffsetAlignment - 1)) == 0 &&
            "minOffsetAlignment 必须是 2 的幂");

        if (minOffsetAlignment > 0) {
            // = [instanceSize / minOffsetAlignment] * minOffsetAlignment ((17+15)&~15 = 32)
            return (instanceSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1);
        }
        return instanceSize;
    }

    CzxBuffer::CzxBuffer(
        CzxDevice& device,
        VkDeviceSize instanceSize, uint32_t instanceCount,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags memoryProps,
        VkDeviceSize minOffsetAlignment)
        : m_device{ device },
        m_instanceSize{ instanceSize },
        m_instanceCount{ instanceCount },
        m_usage{ usage },
        m_memoryProps{ memoryProps } {

        m_alignmentSize = getAlignment(instanceSize, minOffsetAlignment);
        m_bufferSize = m_alignmentSize * instanceCount;
        device.createBuffer(m_bufferSize, usage, memoryProps, m_buffer, m_bufferMemory);
    }

    CzxBuffer::~CzxBuffer() {
        unmap();
        vkDestroyBuffer(m_device.device(), m_buffer, nullptr);
        vkFreeMemory(m_device.device(), m_bufferMemory, nullptr);
    }

    VkResult CzxBuffer::map(VkDeviceSize size, VkDeviceSize offset) {
        assert(m_buffer && m_bufferMemory && "Called map on buffer before create");
        return vkMapMemory(m_device.device(), m_bufferMemory, offset, size, 0, &m_mapped);
    }


    void CzxBuffer::unmap() {
        if (m_mapped) {
            vkUnmapMemory(m_device.device(), m_bufferMemory);
            m_mapped = nullptr;
        }
    }

    void CzxBuffer::writeToBuffer(void* data, VkDeviceSize size, VkDeviceSize offset) {
        assert(m_mapped && "Cannot copy to unmapped buffer");

        if (size == VK_WHOLE_SIZE) {
            memcpy(m_mapped, data, m_bufferSize);
        }
        else {
            char* memOffset = (char*)m_mapped;
            memOffset += offset;
            memcpy(memOffset, data, size);
        }
    }

    VkResult CzxBuffer::flush(VkDeviceSize size, VkDeviceSize offset) {
        VkMappedMemoryRange mappedRange = {
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .memory = m_bufferMemory,
            .offset = offset,
            .size = size
        };
        return vkFlushMappedMemoryRanges(m_device.device(), 1, &mappedRange);
    }

    VkResult CzxBuffer::invalidate(VkDeviceSize size, VkDeviceSize offset) {
        VkMappedMemoryRange mappedRange = {
            .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .memory = m_bufferMemory,
            .offset = offset,
            .size = size
        };
        return vkInvalidateMappedMemoryRanges(m_device.device(), 1, &mappedRange);
    }

    VkDescriptorBufferInfo CzxBuffer::descriptorInfo(VkDeviceSize size, VkDeviceSize offset) {
        return VkDescriptorBufferInfo{
            m_buffer,
            offset,
            size,
        };
    }

    void CzxBuffer::writeToIndex(void* data, int index) {
        writeToBuffer(data, m_instanceSize, index * m_alignmentSize);
    }

    VkResult CzxBuffer::flushIndex(int index) { return flush(m_alignmentSize, index * m_alignmentSize); }

    VkDescriptorBufferInfo CzxBuffer::descriptorInfoForIndex(int index) {
        return descriptorInfo(m_alignmentSize, index * m_alignmentSize);
    }

    VkResult CzxBuffer::invalidateIndex(int index) {
        return invalidate(m_alignmentSize, index * m_alignmentSize);
    }

}  // namespace czx