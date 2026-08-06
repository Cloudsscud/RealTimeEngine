#pragma once

#include "czx_device.h"

// std
#include <memory>
#include <unordered_map>
#include <vector>

namespace czx {

    //
    class CzxDescriptorSetLayout {
    public:
        // 辅助类，用于创建管道所需的蓝图对象
        class Builder {
        public:
            Builder(CzxDevice& device) : m_device{ device } {}

            // 将所需信息添加到绑定映射中，返回引用方便连续调用
            Builder& addBinding(
                uint32_t binding,
                VkDescriptorType descriptorType,    // 期望的描述符类型(统一缓冲、存储缓冲、图像缓冲等)
                VkShaderStageFlags stageFlags,      // 访问该绑定的着色器阶段
                uint32_t count = 1);    // 该绑定包含的描述符数量
            std::unique_ptr<CzxDescriptorSetLayout> build() const;

        private:
            CzxDevice& m_device;
            std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> m_bindings{};    // 绑定的无序映射
        };

        CzxDescriptorSetLayout(
            CzxDevice& czxDevice, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings);
        ~CzxDescriptorSetLayout();
        CzxDescriptorSetLayout(const CzxDescriptorSetLayout&) = delete;
        CzxDescriptorSetLayout& operator=(const CzxDescriptorSetLayout&) = delete;

        VkDescriptorSetLayout getDescriptorSetLayout() const { return m_descriptorSetLayout; }

    private:
        CzxDevice& m_device;
        VkDescriptorSetLayout m_descriptorSetLayout;
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> m_bindings;

        friend class CzxDescriptorWriter;
    };

    // 描述符池
    class CzxDescriptorPool {
    public:
        class Builder {
        public:
            Builder(CzxDevice& czxDevice) : m_device{ czxDevice } {}

            // 允许链式调用
            Builder& addPoolSize(VkDescriptorType descriptorType, uint32_t count);  // 添加多个描述符
            Builder& setPoolFlags(VkDescriptorPoolCreateFlags flags);   // 设置池对象行为
            Builder& setMaxSets(uint32_t count);    // 最大可分配描述符个数
            std::unique_ptr<CzxDescriptorPool> build() const;

        private:
            CzxDevice& m_device;
            std::vector<VkDescriptorPoolSize> m_poolSizes{};
            uint32_t m_maxSets = 1000;
            VkDescriptorPoolCreateFlags m_poolFlags = 0;
        };

        CzxDescriptorPool(
            CzxDevice& czxDevice,
            uint32_t maxSets,
            VkDescriptorPoolCreateFlags poolFlags,
            const std::vector<VkDescriptorPoolSize>& poolSizes);
        ~CzxDescriptorPool();
        CzxDescriptorPool(const CzxDescriptorPool&) = delete;
        CzxDescriptorPool& operator=(const CzxDescriptorPool&) = delete;

        // 分配描述符
        bool allocateDescriptor(
            const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet& descriptor) const;
        // 释放描述符
        void freeDescriptors(std::vector<VkDescriptorSet>& descriptors) const;
        // 重置描述符池
        void resetPool();

    private:
        CzxDevice& m_device;
        VkDescriptorPool m_descriptorPool;

        friend class CzxDescriptorWriter;
    };

    // 负责构建实际的描述符
    class CzxDescriptorWriter {
    public:
        CzxDescriptorWriter(CzxDescriptorSetLayout& setLayout, CzxDescriptorPool& pool);

        CzxDescriptorWriter& writeBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo);
        CzxDescriptorWriter& writeImage(uint32_t binding, VkDescriptorImageInfo* imageInfo);

        bool build(VkDescriptorSet& set);
        void overwrite(VkDescriptorSet& set);

    private:
        CzxDescriptorSetLayout& m_setLayout;
        CzxDescriptorPool& m_pool;
        std::vector<VkWriteDescriptorSet> m_writes;
    };

}  // namespace czx