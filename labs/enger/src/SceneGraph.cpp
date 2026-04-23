#include "SceneGraph.h"

#include "vulkan/vk.h"

#include "vulkan/Device.h"

#include <fstream>
#include <expected>
#include <filesystem>

#include "Renderer.h"

#include "MeshLoader.h"
#include "Logging/Log.h"
#include "Utils/Spirv.h"

namespace enger
{
    void GLTFMetallic_Roughness::buildPipelines(Device& device, vk::Format drawFormat, vk::Format depthFormat, vk::SampleCountFlagBits msaaSamples, ShaderModuleHandle
                                                shaderOverride)
    {
        ENGER_PROFILE_FUNCTION()

        ShaderModuleHandle shaderModule = shaderOverride;
        Holder<ShaderModuleHandle> shaderModuleHolder{};

        if (!shaderModule.valid())
        {
            auto expectedShaderData = loadSpirvFromFile("shaders/mesh.spv");
            if (!expectedShaderData)
            {
                LOG_ERROR("Failed to load shader data: {}", expectedShaderData.error());
                std::terminate();
            }

            shaderModuleHolder = device.createShaderModule(std::move(expectedShaderData.value()),
                nullptr, "GLTFMetallic_Roughness: MeshShaderModule");
            shaderModule = shaderModuleHolder;
        }

        opaquePipeline.pipelineLayout = device.bindlessGraphicsPipelineLayout();
        opaquePipeline.pipeline = device.createGraphicsPipeline(GraphicsPipelineDesc{
            .pipelineLayout = opaquePipeline.pipelineLayout,
            .vertexShaderModule = shaderModule,
            .fragmentShaderModule = shaderModule,
            .depthTestEnable = true,
            .depthWriteEnable = true,

            .depthCompareOp = vk::CompareOp::eGreaterOrEqual,
            .colorAttachments = {
                ColorAttachment{
                    .format = drawFormat,
                    .blendEnabled = false,
                }
            },
            .depthFormat = depthFormat,

            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eCounterClockwise,

            .sampleCount = msaaSamples,

#ifndef NDEBUG
            .enablePipelineRobustness = true,
#endif
        }, nullptr, "GLTFMetallic_Roughness: OpaquePipeline");

        // For now, the unlit pipeline is the SAME as the opaque pipeline
        unlitPipeline.pipelineLayout = device.bindlessGraphicsPipelineLayout();
        unlitPipeline.pipeline = device.createGraphicsPipeline(GraphicsPipelineDesc{
            .pipelineLayout = unlitPipeline.pipelineLayout,
            .vertexShaderModule = shaderModule,
            .fragmentShaderModule = shaderModule,
            .depthTestEnable = true,
            .depthWriteEnable = true,

            .depthCompareOp = vk::CompareOp::eGreaterOrEqual,
            .colorAttachments = {
                ColorAttachment{
                    .format = drawFormat,
                    .blendEnabled = false,
                }
            },
            .depthFormat = depthFormat,

            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eCounterClockwise,

            .sampleCount = msaaSamples,

#ifndef NDEBUG
            .enablePipelineRobustness = true,
#endif
        }, nullptr, "GLTFMetallic_Roughness: UnlitPipeline");

        additivePipeline.pipelineLayout = device.bindlessGraphicsPipelineLayout();
        additivePipeline.pipeline = device.createGraphicsPipeline(GraphicsPipelineDesc{
            .pipelineLayout = additivePipeline.pipelineLayout,
            .vertexShaderModule = shaderModule,
            .fragmentShaderModule = shaderModule,
            .depthTestEnable = true,
            .depthWriteEnable = false,

            .depthCompareOp = vk::CompareOp::eGreaterOrEqual,
            .colorAttachments = {
                ColorAttachment{ // enable additive blending
                    .format = drawFormat,
                    .blendEnabled = true,
                    .srcRgbBlendFactor = vk::BlendFactor::eSrcAlpha,
                    .dstRgbBlendFactor = vk::BlendFactor::eOne,
                    .rgbBlendOp = vk::BlendOp::eAdd,
                    .srcAlphaBlendFactor = vk::BlendFactor::eOne,
                    .dstAlphaBlendFactor = vk::BlendFactor::eZero,
                    .alphaBlendOp = vk::BlendOp::eAdd,
                },
            },

            .depthFormat = depthFormat,

            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eCounterClockwise,

            .sampleCount = msaaSamples,

#ifndef NDEBUG
            .enablePipelineRobustness = true,
#endif
        }, nullptr, "GLTFMetallic_Roughness: AdditivePipeline");

        transparentPipeline.pipelineLayout = device.bindlessGraphicsPipelineLayout();
        transparentPipeline.pipeline = device.createGraphicsPipeline(GraphicsPipelineDesc{
            .pipelineLayout = transparentPipeline.pipelineLayout,
            .vertexShaderModule = shaderModule,
            .fragmentShaderModule = shaderModule,
            .depthTestEnable = true,
            .depthWriteEnable = false,

            .depthCompareOp = vk::CompareOp::eGreaterOrEqual,
            .colorAttachments = {
                ColorAttachment{ // enable alpha blending
                    .format = drawFormat,
                    .blendEnabled = true,
                    .srcRgbBlendFactor = vk::BlendFactor::eSrcAlpha,
                    .dstRgbBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
                    .rgbBlendOp = vk::BlendOp::eAdd,
                    .srcAlphaBlendFactor = vk::BlendFactor::eOne,
                    .dstAlphaBlendFactor = vk::BlendFactor::eZero,
                    .alphaBlendOp = vk::BlendOp::eAdd,
                },
            },

            .depthFormat = depthFormat,

            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eCounterClockwise,

            .sampleCount = msaaSamples,

#ifndef NDEBUG
            .enablePipelineRobustness = true,
#endif
        }, nullptr, "GLTFMetallic_Roughness: TransparentPipeline");
    }

    MaterialInstance GLTFMetallic_Roughness::writeMaterial(MaterialPass pass,
                                                           MaterialResources&& resources)
    {
        ENGER_PROFILE_FUNCTION()
        MaterialInstance inst;
        inst.passType = pass;
        switch (pass)
        {
            case MaterialPass::MainColor:
                inst.pipeline = &opaquePipeline;
                break;
            case MaterialPass::Unlit:
                inst.pipeline = &unlitPipeline;
                break;
            case MaterialPass::Additive:
                inst.pipeline = &additivePipeline;
                break;
            case MaterialPass::Transparent:
                inst.pipeline = &transparentPipeline;
                break;
            default:
                LOG_ERROR("Unknown pass type: {}", static_cast<uint32_t>(pass));
                inst.pipeline = nullptr;
                break;
        }
        inst.resources = std::move(resources);

        return inst;
    }

    void MeshNode::draw(const glm::mat4& topMatrix, DrawContext& ctx)
    {
        ENGER_PROFILE_FUNCTION()
        glm::mat4 nodeMatrix = topMatrix * worldTransform;

        for (auto& s : mesh->surfaces)
        {

            RenderObject obj;
            obj.indexCount = s.indexCount;
            obj.firstIndex = s.startIndex;
            obj.indexBuffer = mesh->meshBuffers.indexBuffer;
            obj.material = &s.material->material;

            obj.transform = nodeMatrix;
            obj.vertexBuffer = mesh->meshBuffers.vertexBuffer;

            if (!obj.material)
                continue;
            switch (obj.material->passType)
            {
                case MaterialPass::MainColor:
                    ctx.opaqueSurfaces.push_back(std::move(obj));
                    break;
                case MaterialPass::Unlit:
                    ctx.unlitSurfaces.push_back(std::move(obj));
                    break;
                case MaterialPass::Additive:
                    ctx.additiveSurfaces.push_back(std::move(obj));
                    break;
                case MaterialPass::Transparent:
                    ctx.transparentSurfaces.push_back(std::move(obj));
                    break;
                default:
                    LOG_ERROR("Unknown pass type: {}", static_cast<uint32_t>(obj.material->passType));
                    break;
            }
        }

        Node::draw(topMatrix, ctx);
    }
}
