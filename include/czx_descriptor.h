#pragma once

#include "czx_device.h"

// std
#include <memory>
#include <unordered_map>
#include <vector>

namespace czx {

    class CzxDescriptorSetLayout {
    public:
        // 辅助类，用于创建管道所需的蓝图对象
        class Builder {
        public:
            Builder(CzxDevice& device) : m_device{ device } {}

            /** 为描述符集布局增加描述符集绑定信息
              * @param binding : 绑定索引 着色器使用的绑定索引
              * @param descriptorType ：该描述符的资源类型(image/buffer...)
              * @param stageFlags ：该描述符资源在着色器中的使用阶段(vertex/fragment/...)
              * @param count ：optional default=1 该描述符资源使用数量，与shader资源数组大小相同
            */
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

            Builder& addPoolSize(VkDescriptorType descriptorType, uint32_t count);  // 添加多个同类型描述符
            Builder& setPoolFlags(VkDescriptorPoolCreateFlags flags);   // 设置池对象行为
            Builder& setMaxSets(uint32_t count);    // 最大可分配描述符集的个数
            std::unique_ptr<CzxDescriptorPool> build() const;

        private:
            CzxDevice& m_device;
            std::vector<VkDescriptorPoolSize> m_poolSizes{};    // 计划分配的各描述符的类型及其数量
            uint32_t m_maxSets = 1000;  // 从该池中分配的描述符集的最大数量
            VkDescriptorPoolCreateFlags m_poolFlags = 0;    // 默认不分配和释放
        };

        CzxDescriptorPool(
            CzxDevice& czxDevice,
            uint32_t maxSets,
            VkDescriptorPoolCreateFlags poolFlags,
            const std::vector<VkDescriptorPoolSize>& poolSizes);
        ~CzxDescriptorPool();
        // 禁赋值与拷贝
        CzxDescriptorPool(const CzxDescriptorPool&) = delete;
        CzxDescriptorPool& operator=(const CzxDescriptorPool&) = delete;

        /** 基于池和相应的描述符集布局来分配单个/多个使用相同布局的描述符集内存
          * @param descriptorSetLayout : 共享的描述符集布局
          * @param descriptorSetCount ：描述符集分配数量
          * @param descriptors ：接收分配的描述符集
        */
        bool allocateDescriptorSets(const VkDescriptorSetLayout& descriptorSetLayout, int descriptorSetCount, std::vector<VkDescriptorSet>& descriptors) const;
        // 释放描述符
        void freeDescriptors(std::vector<VkDescriptorSet>& descriptors) const;
        // 重置描述符池
        void resetPool();

    private:
        CzxDevice& m_device;
        VkDescriptorPool m_descriptorPool;

        friend class CzxDescriptorWriter;
    };


    class CzxDescriptorWriter {
    public:
        CzxDescriptorWriter(CzxDescriptorSetLayout& setLayout, CzxDescriptorPool& pool);

        CzxDescriptorWriter& writeBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo);
        CzxDescriptorWriter& writeImage(uint32_t binding, const VkDescriptorImageInfo* imageInfo);

        // 默认只分配并更新一个描述符集的数据
        bool build(std::vector<VkDescriptorSet>& descriptorSets,int descriptorSetCount = 1);
        void updateDescriptorSets(std::vector<VkDescriptorSet>& descriptorSets);

    private:
        CzxDescriptorSetLayout& m_setLayout;
        CzxDescriptorPool& m_pool;
        std::vector<VkWriteDescriptorSet> m_writes;
    };

}  // namespace czx