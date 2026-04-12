#pragma once

#include "vk.h"

#include "Resources.h"

namespace enger
{
    struct ColorAttachment
    {
        vk::Format format = vk::Format::eUndefined;
        bool blendEnabled = false;
        vk::BlendFactor srcRgbBlendFactor = vk::BlendFactor::eOne;
        vk::BlendFactor dstRgbBlendFactor = vk::BlendFactor::eZero;
        vk::BlendOp rgbBlendOp = vk::BlendOp::eAdd;
        vk::BlendFactor srcAlphaBlendFactor = vk::BlendFactor::eZero;
        vk::BlendFactor dstAlphaBlendFactor = vk::BlendFactor::eOne;
        vk::BlendOp alphaBlendOp = vk::BlendOp::eAdd;
        vk::ColorComponentFlags colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    };

    struct ComputePipelineDesc
    {
        PipelineLayoutHandle pipelineLayout;
        ShaderModuleHandle shaderModule;

        std::string entryPoint = "computeMain";
    };

    struct GraphicsPipelineDesc
    {
        PipelineLayoutHandle pipelineLayout;

        ShaderModuleHandle vertexShaderModule;
        ShaderModuleHandle fragmentShaderModule;

        std::string entryPointVertex = "vertexMain";
        std::string entryPointFragment = "fragmentMain";

        bool depthTestEnable = false;
        bool depthWriteEnable = false;
        vk::CompareOp depthCompareOp = vk::CompareOp::eLessOrEqual;

        static constexpr auto kMaxColorAttachments = 8;
        std::array<ColorAttachment, kMaxColorAttachments> colorAttachments;
        uint32_t colorAttachmentCount = 0;

        vk::Format depthFormat = vk::Format::eUndefined;
        vk::Format stencilFormat = vk::Format::eUndefined;

        vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;
        vk::CullModeFlags cullMode = vk::CullModeFlagBits::eBack;
        vk::FrontFace frontFace = vk::FrontFace::eClockwise;
        vk::PolygonMode polygonMode = vk::PolygonMode::eFill;

        vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1;
        float minSampleShading = 0.0f;
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
        std::span<const DescriptorSetLayoutHandle> descriptorLayouts;
        std::span<const PushConstantsInfo> pushConstantRanges;
    };

    struct PipelineLayout
    {
        vk::PipelineLayout layout;
    };
}
