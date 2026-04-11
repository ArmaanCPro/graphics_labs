#include "Descriptors.h"

#include "Device.h"

namespace enger
{
    void DescriptorAllocator::initPool(vk::Device device, uint32_t maxSets, std::span<PoolSizeRatio> poolSizeRatios)
    {
        std::vector<vk::DescriptorPoolSize> poolSizes;
        for (auto ratio : poolSizeRatios)
        {
            poolSizes.push_back(vk::DescriptorPoolSize{
                .type = ratio.type_,
                .descriptorCount = static_cast<uint32_t>(ratio.ratio_ * maxSets),
            });
        }

        vk::DescriptorPoolCreateInfo poolCI{
            .flags = {},
            .maxSets = maxSets,
            .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
            .pPoolSizes = poolSizes.data(),
        };

        vkCheck(device.createDescriptorPool(&poolCI, nullptr, &pool_));
    }

    void DescriptorAllocator::clearDescriptors(vk::Device device)
    {
        vkCheck(device.resetDescriptorPool(pool_, vk::DescriptorPoolResetFlags{}));
    }

    void DescriptorAllocator::destroyPool(vk::Device device)
    {
        device.destroyDescriptorPool(pool_);
    }

    vk::DescriptorSet DescriptorAllocator::allocate(Device& device, DescriptorSetLayoutHandle handle)
    {
        auto* layout = device.getDescriptorSetLayout(handle);
        assert(layout != nullptr);

        vk::DescriptorSetAllocateInfo allocInfo{
            .descriptorPool = pool_,
            .descriptorSetCount = 1,
            .pSetLayouts = layout,
        };

        vk::DescriptorSet set{};
        vkCheck(device.device().allocateDescriptorSets(&allocInfo, &set));

        return set;
    }
}
