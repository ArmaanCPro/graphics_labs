#pragma once

#include "vk.h"

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

    struct PipelineLayoutDesc
    {
        std::span<DescriptorSetLayoutHandle> descriptorLayouts;
    };

    struct PipelineLayout
    {
        vk::PipelineLayout layout;
    };
}
