#include "SceneGraph.h"

#include "vulkan/vk.h"

#include "vulkan/Device.h"

#include <fstream>
#include <expected>
#include <filesystem>

#include "Renderer.h"

#include "MeshLoader.h"
#include "Utils/Spirv.h"

namespace enger
{
    void GLTFMetallic_Roughness::buildPipelines(Device& device, vk::Format drawFormat, vk::Format depthFormat, vk::SampleCountFlagBits msaaSamples)
    {
        ENGER_PROFILE_FUNCTION()
        auto expectedShaderData = loadSpirvFromFile("shaders/mesh.spv");
        if (!expectedShaderData)
        {
            std::cerr << "Failed to load shader data: " << expectedShaderData.error();
            std::terminate();
        }

        Holder<ShaderModuleHandle> shaderModule = device.createShaderModule(std::move(expectedShaderData.value()),
            nullptr, "GLTFMetallic_Roughness: MeshShaderModule");

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
            .colorAttachmentCount = 1,
            .depthFormat = depthFormat,

            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eCounterClockwise,

            .sampleCount = msaaSamples,

#ifndef NDEBUG
            .enablePipelineRobustness = true,
#endif
        }, nullptr, "GLTFMetallic_Roughness: OpaquePipeline");

        transparentPipeline.pipelineLayout = device.bindlessGraphicsPipelineLayout();
        transparentPipeline.pipeline = device.createGraphicsPipeline(GraphicsPipelineDesc{
            .pipelineLayout = transparentPipeline.pipelineLayout,
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
            .colorAttachmentCount = 1,

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
        inst.pipeline = pass == MaterialPass::MainColor ? &opaquePipeline : &transparentPipeline;
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
            if (obj.material->passType == MaterialPass::Transparent)
                ctx.transparentSurfaces.push_back(std::move(obj));
            else if (obj.material->passType == MaterialPass::MainColor)
                ctx.opaqueSurfaces.push_back(std::move(obj));
        }

        Node::draw(topMatrix, ctx);
    }
}
