#include "czx_descriptor.h"

// std
#include <cassert>
#include <stdexcept>
#include <czx_utils.h>

namespace czx {

    // *************** Descriptor Set Layout Builder *********************

    CzxDescriptorSetLayout::Builder& CzxDescriptorSetLayout::Builder::addBinding(
        uint32_t binding,
        VkDescriptorType descriptorType,
        VkShaderStageFlags stageFlags,
        uint32_t count) {
        // 检查该绑定索引是否已绑定过
        assert(m_bindings.count(binding) == 0 && "Binding already in use");
        VkDescriptorSetLayoutBinding layoutBinding{
            .binding = binding, // 该布局的绑定索引
            .descriptorType = descriptorType,   // 该描述符的资源类型
            .descriptorCount = count,   // 该描述符的资源数量，用于shader内创建数组
            .stageFlags = stageFlags   // 该描述符资源在着色器的使用阶段
        };

        m_bindings[binding] = layoutBinding;
        return *this;
    }

    std::unique_ptr<CzxDescriptorSetLayout> CzxDescriptorSetLayout::Builder::build() const {
        return std::make_unique<CzxDescriptorSetLayout>(m_device, m_bindings);
    }

    // *************** Descriptor Set Layout *********************

    CzxDescriptorSetLayout::CzxDescriptorSetLayout(
        CzxDevice& device, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings)
        : m_device{ device }, m_bindings{ bindings } {

        // 所有描述符的绑定布局
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
        for (auto& kv : bindings) {
            setLayoutBindings.push_back(kv.second);
        }

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .bindingCount = static_cast<uint32_t>(setLayoutBindings.size()),
            .pBindings = setLayoutBindings.data()       // 该布局中的各个描述符绑定顺序(shader的binding值)及其资源类型
        };

        VkResult result = vkCreateDescriptorSetLayout(device.device(),&descriptorSetLayoutInfo,nullptr,&m_descriptorSetLayout);
        CHECK_VK_RESULT(result, "create descriptor set layout");
        printf("descriptor set layout created\n");
    }

    CzxDescriptorSetLayout::~CzxDescriptorSetLayout() {
        vkDestroyDescriptorSetLayout(m_device.device(), m_descriptorSetLayout, nullptr);
    }

    // *************** Descriptor Pool Builder *********************

    // 为该种资源类型的描述符预分配的数量
    CzxDescriptorPool::Builder& CzxDescriptorPool::Builder::addPoolSize(
        VkDescriptorType descriptorType, uint32_t count) {
        m_poolSizes.push_back({ descriptorType, count });
        return *this;
    }
    // 设置 描述符集 额外处理操作(释放~FREE_DESCRIPTOR_SET_BIT)
    CzxDescriptorPool::Builder& CzxDescriptorPool::Builder::setPoolFlags(
        VkDescriptorPoolCreateFlags flags) {
        m_poolFlags = flags;
        return *this;
    }
    // 设置最大允许分配的描述符集数量
    CzxDescriptorPool::Builder& CzxDescriptorPool::Builder::setMaxSets(uint32_t count) {
        m_maxSets = count;
        return *this;
    }
    // 正式构建描述符池
    std::unique_ptr<CzxDescriptorPool> CzxDescriptorPool::Builder::build() const {
        return std::make_unique<CzxDescriptorPool>(m_device, m_maxSets, m_poolFlags, m_poolSizes);
    }

    // *************** Descriptor Pool *********************

    CzxDescriptorPool::CzxDescriptorPool(
        CzxDevice& device,
        uint32_t maxSets,
        VkDescriptorPoolCreateFlags poolFlags,
        const std::vector<VkDescriptorPoolSize>& poolSizes)
        : m_device{ device } {

        VkDescriptorPoolCreateInfo descriptorPoolInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .flags = poolFlags,
            .maxSets = maxSets,
            .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
            .pPoolSizes = poolSizes.data()
        };

        VkResult result = vkCreateDescriptorPool(device.device(), &descriptorPoolInfo, nullptr, &m_descriptorPool);
        CHECK_VK_RESULT(result, "create descriptor pool");
        printf("descriptor pool created\n");
    }

    CzxDescriptorPool::~CzxDescriptorPool() {
        vkDestroyDescriptorPool(m_device.device(), m_descriptorPool, nullptr);
    }

    bool CzxDescriptorPool::allocateDescriptorSets(const VkDescriptorSetLayout& descriptorSetLayout, int descriptorSetCount, std::vector<VkDescriptorSet>& descriptors)
        const {

        std::vector<VkDescriptorSetLayout> descriptorSetLayouts(descriptorSetCount, descriptorSetLayout);

        VkDescriptorSetAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = nullptr,
            .descriptorPool = m_descriptorPool, // 参与分配并避免内存碎片的池
            .descriptorSetCount = (uint32_t)descriptorSetCount,   // 分配描述符集的数量
            .pSetLayouts = descriptorSetLayouts.data()   // 每个描述符集的布局数据
        };

        descriptors.resize(descriptorSetCount);

        VkResult result = vkAllocateDescriptorSets(m_device.device(), &allocInfo, descriptors.data());
        CHECK_VK_RESULT(result, "allocate descriptorSets");

        return true;
    }

    void CzxDescriptorPool::freeDescriptors(std::vector<VkDescriptorSet>& descriptors) const {
        vkFreeDescriptorSets(
            m_device.device(),
            m_descriptorPool,
            static_cast<uint32_t>(descriptors.size()),
            descriptors.data());
    }

    void CzxDescriptorPool::resetPool() {
        vkResetDescriptorPool(m_device.device(), m_descriptorPool, 0);
    }

    // *************** Descriptor Writer *********************

    CzxDescriptorWriter::CzxDescriptorWriter(CzxDescriptorSetLayout& setLayout, CzxDescriptorPool& pool)
        : m_setLayout{ setLayout }, m_pool{ pool } {
    }

    CzxDescriptorWriter& CzxDescriptorWriter::writeBuffer(
        uint32_t binding, VkDescriptorBufferInfo* bufferInfo) {
        assert(m_setLayout.m_bindings.count(binding) == 1 && "Layout does not contain specified binding");

        auto& setLayoutBinding = m_setLayout.m_bindings[binding];

        for (int i = 0; i < setLayoutBinding.descriptorCount; ++i) {
            VkWriteDescriptorSet write{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstBinding = binding,  // 描述符集布局中的使用的绑定点
                .dstArrayElement = 0,   // shader目标数组的索引，用于特定更新特定索引的资源
                .descriptorCount = 1,   // 从绑定点开始，向后一次性更新的绑定点列表
                .descriptorType = setLayoutBinding.descriptorType,
                .pBufferInfo = bufferInfo   // 实际被更新的缓冲区信息
            };
            m_writes.push_back(write);

        }

        return *this;
    }

    CzxDescriptorWriter& CzxDescriptorWriter::writeImage(
        uint32_t binding, const VkDescriptorImageInfo* imageInfo) {
        assert(m_setLayout.m_bindings.count(binding) == 1 && "Layout does not contain specified binding");

        auto& bindingDescription = m_setLayout.m_bindings[binding];

        assert(
            bindingDescription.descriptorCount == 1 &&
            "Binding single descriptor info, but binding expects multiple");

        VkWriteDescriptorSet write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = binding,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = bindingDescription.descriptorType,
            .pImageInfo = imageInfo
        };

        m_writes.push_back(write);
        return *this;
    }

    bool CzxDescriptorWriter::build(std::vector<VkDescriptorSet>& descriptorSets, int descriptorSetCount) {
        bool success = m_pool.allocateDescriptorSets(m_setLayout.getDescriptorSetLayout(), descriptorSetCount, descriptorSets);
        if (!success) {
            return false;
        }
        updateDescriptorSets(descriptorSets);
        return true;
    }

    void CzxDescriptorWriter::updateDescriptorSets(std::vector<VkDescriptorSet>& descriptorSets) {
        for (size_t i = 0; i < descriptorSets.size();++i) {
            m_writes[i].dstSet = descriptorSets[i];
        }
        vkUpdateDescriptorSets(m_pool.m_device.device(), m_writes.size(), m_writes.data(), 0, nullptr);
    }

}  // namespace czx