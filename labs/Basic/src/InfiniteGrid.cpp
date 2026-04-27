#include "InfiniteGrid.h"

#include "Utils/Spirv.h"
#include "vulkan/Pipeline.h"

#include "vulkan/Device.h"

InfiniteGrid::InfiniteGrid(enger::Device& device, vk::SampleCountFlagBits samples, vk::Format renderFormat, vk::Format depthFormat)
{
    m_GridPipelineLayout = device.createPipelineLayout(enger::PipelineLayoutDesc{
                                                             .descriptorLayouts = {
                                                                 {device.bindlessDescriptorSetLayout()}
                                                             },
                                                             .pushConstantRanges = {
                                                                 {
                                                                     enger::PushConstantsInfo{
                                                                         .offset = 0,
                                                                         .size = sizeof(GridPushConstants),
                                                                         .stages =
                                                                         vk::ShaderStageFlagBits::eAllGraphics
                                                                     }
                                                                 }
                                                             }
                                                         }, &device.graphicsQueue(), "Grid Pipeline Layout");
    auto gridSpirv = enger::loadSpirvFromFile("shaders/Grid.spv");
    EASSERT(gridSpirv.has_value());
    auto gridSM = device.createShaderModule(std::move(gridSpirv.value()), &device.graphicsQueue(), "Grid Shader Module");
    m_GridPipeline = device.createGraphicsPipeline(enger::GraphicsPipelineDesc{
                                                         .pipelineLayout = m_GridPipelineLayout,
                                                         .vertexShaderModule = gridSM,
                                                         .fragmentShaderModule = gridSM,
                                                         .depthTestEnable = true,
                                                         .depthWriteEnable = false,
                                                         .depthCompareOp = vk::CompareOp::eGreaterOrEqual,
                                                         .colorAttachments = {
                                                             enger::ColorAttachment{
                                                                 .format = renderFormat,
                                                                 .blendEnabled = true,
                                                                 .srcRgbBlendFactor = vk::BlendFactor::eSrcAlpha,
                                                                 .dstRgbBlendFactor =
                                                                 vk::BlendFactor::eOneMinusSrcAlpha,
                                                                 .rgbBlendOp = vk::BlendOp::eAdd,
                                                                 .srcAlphaBlendFactor = vk::BlendFactor::eOne,
                                                                 .dstAlphaBlendFactor = vk::BlendFactor::eOne,
                                                                 .alphaBlendOp = vk::BlendOp::eAdd,
                                                             }
                                                         },
                                                         .depthFormat = depthFormat,

                                                         .cullMode = vk::CullModeFlagBits::eNone,

                                                         .sampleCount = samples,

#ifndef NDEBUG
                                                         .enablePipelineRobustness = true,
#endif
                                                     }, &device.graphicsQueue(), "Grid Pipeline");
}

enger::ProceduralDrawObject InfiniteGrid::draw(const enger::DrawContext& dctx)
{
    GridPushConstants gpc{
        .mvp = dctx.viewProj,
        .camPos = glm::vec4(dctx.cameraPos, 1.0f),
        .origin = glm::vec4{0.0f},
    };

    std::array<std::byte, 128> pushConstantsData;
    EASSERT(sizeof(gpc) <= pushConstantsData.size());
    std::memcpy(pushConstantsData.data(), &gpc, sizeof(gpc));

    return enger::ProceduralDrawObject{
        .pipeline = m_GridPipeline,
        .pipelineLayout = m_GridPipelineLayout,
        .vertexCount = 6,
        .pushConstantsSize = sizeof(GridPushConstants),
        .pushConstantsData = std::move(pushConstantsData)
    };
}
