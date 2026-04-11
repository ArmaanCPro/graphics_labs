#pragma once

#include "vk.h"

#include "Resources.h"

namespace enger
{
    struct ComputePipelineDesc
    {
        ShaderModuleHandle shaderModule;
        PipelineLayoutHandle pipelineLayout;
    };

    struct Pipeline
    {
        vk::Pipeline handle;
        //vk::PipelineLayout layout;
        //vk::PipelineBindPoint bindPoint;
    };

    struct PushConstantsInfo
    {
        uint32_t offset;
        uint32_t size;
        vk::ShaderStageFlags stages;

        explicit operator vk::PushConstantRange() const
        {
            return vk::PushConstantRange{ stages, offset, size };
        }
    };

    struct PipelineLayoutDesc
    {
        std::span<DescriptorSetLayoutHandle> descriptorLayouts;
        std::span<PushConstantsInfo> pushConstantRanges;
    };

    struct PipelineLayout
    {
        vk::PipelineLayout layout;
    };
}
