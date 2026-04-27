#include "Renderer.h"

#include <execution>
#include <expected>
#include <fstream>
#include <filesystem>

#include "vulkan/QueueSubmitBuilder.h"

#include "Scene/SceneGraph.h"

#include "MeshLoader.h"
#include "Logging/Assert.h"
#include "Utils/Spirv.h"
#include "vulkan/PushConstantRanges.h"

#include "vulkan/Commands.h"

namespace enger
{
    Renderer::Renderer(Device& device, SwapChain& swapchain) :
        m_Device(device),
        m_SwapChain(swapchain),
        m_GraphicsQueue(device.graphicsQueue())
    {
        // Create textures
        createRenderTextures(m_SwapChain.swapChainExtent().width, m_SwapChain.swapChainExtent().height);

        // Create grid pipeline

        m_GridPipelineLayout = m_Device.createPipelineLayout(PipelineLayoutDesc{
                                                                 .descriptorLayouts = {
                                                                     {m_Device.bindlessDescriptorSetLayout()}
                                                                 },
                                                                 .pushConstantRanges = {
                                                                     {
                                                                         PushConstantsInfo{
                                                                             .offset = 0,
                                                                             .size = sizeof(GridPushConstants),
                                                                             .stages =
                                                                             vk::ShaderStageFlagBits::eAllGraphics
                                                                         }
                                                                     }
                                                                 }
                                                             }, &m_GraphicsQueue, "Grid Pipeline Layout");
        auto gridSpirv = loadSpirvFromFile("shaders/Grid.spv");
        EASSERT(gridSpirv.has_value());
        auto gridSM = m_Device.createShaderModule(std::move(gridSpirv.value()), &m_GraphicsQueue, "Grid Shader Module");
        m_GridPipeline = m_Device.createGraphicsPipeline(GraphicsPipelineDesc{
                                                             .pipelineLayout = m_GridPipelineLayout,
                                                             .vertexShaderModule = gridSM,
                                                             .fragmentShaderModule = gridSM,
                                                             .depthTestEnable = true,
                                                             .depthWriteEnable = false,
                                                             .depthCompareOp = vk::CompareOp::eGreaterOrEqual,
                                                             .colorAttachments = {
                                                                 ColorAttachment{
                                                                     .format = m_Device.getImage(m_MsaaRenderTarget)->
                                                                     format_,
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
                                                             .depthFormat = m_Device.getImage(m_DepthBuffer)->format_,

                                                             .cullMode = vk::CullModeFlagBits::eNone,

                                                             .sampleCount = m_MsaaSamples,

#ifndef NDEBUG
                                                             .enablePipelineRobustness = true,
#endif
                                                         }, &m_GraphicsQueue, "Grid Pipeline");


        // Create tonemapping pipeline (could move to a frame graph later on)
        m_TonemapperPipelineLayout = m_Device.createPipelineLayout(PipelineLayoutDesc{
                                                                       .descriptorLayouts = {
                                                                           {m_Device.bindlessDescriptorSetLayout()}
                                                                       },
                                                                       .pushConstantRanges = {
                                                                           {
                                                                               PushConstantsInfo{
                                                                                   .offset = 0,
                                                                                   .size = sizeof(
                                                                                       TonemapperPushConstants),
                                                                                   .stages =
                                                                                   vk::ShaderStageFlagBits::eFragment
                                                                               }
                                                                           }
                                                                       },
                                                                   }, &m_GraphicsQueue, "Tonemapper Pipeline Layout");

        auto tonemapperVertSpirv = loadSpirvFromFile("shaders/TonemappingVertex.spv");
        auto tonemapperSpirv = loadSpirvFromFile("shaders/ACES.spv");
        EASSERT(tonemapperVertSpirv.has_value());
        EASSERT(tonemapperSpirv.has_value());
        auto tonemapperVertSM = m_Device.createShaderModule(std::move(tonemapperVertSpirv.value()), &m_GraphicsQueue,
                                                            "Tonemapper Vertex Shader Module");
        auto tonemapperSM = m_Device.createShaderModule(std::move(tonemapperSpirv.value()), &m_GraphicsQueue,
                                                        "Tonemapper Fragment Shader Module");

        m_TonemapperPipeline = m_Device.createGraphicsPipeline(GraphicsPipelineDesc{
                                                                   .pipelineLayout = m_TonemapperPipelineLayout,
                                                                   .vertexShaderModule = tonemapperVertSM,
                                                                   .fragmentShaderModule = tonemapperSM,


                                                                   .colorAttachments = {
                                                                       ColorAttachment{
                                                                           .format = m_SwapChain.swapChainFormat()
                                                                       }
                                                                   },
                                                                   .cullMode = vk::CullModeFlagBits::eNone,

#ifndef NDEBUG
                                                                   .enablePipelineRobustness = true,
#endif
                                                               }, &m_GraphicsQueue, "Tonemapper Pipeline");
    }

    void Renderer::render(framing::FrameContext& fctx, const DrawContext& dctx, EngineStats& stats, bool drawGrid)
    {
        ENGER_PROFILE_FUNCTION();
        auto* d = &m_Device;
        ENGER_PROFILE_GPU_ZONE("Renderer::render", d, fctx.cmd.get(), ENGER_PROFILE_COLOR_SUBMIT);

        if (m_ShouldResize)
        {
            createRenderTextures(m_PendingWidth, m_PendingHeight);
            m_ShouldResize = false;
        }

        auto& cmd = fctx.cmd;
        auto start = std::chrono::high_resolution_clock::now();

        m_FrameGraph.reset(m_Device, fctx.swapchainImageHandle);

        m_FrameGraph.addPass(buildGeometryPass(dctx, stats));
        if (drawGrid)
            m_FrameGraph.addPass(buildGridPass(dctx, stats));

        m_FrameGraph.addPass(buildTonemapperPass(fctx.swapchainImageHandle));

        m_FrameGraph.compile(m_Device);
        m_FrameGraph.execute(cmd, m_Device);

        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        stats.meshDrawTime = elapsed.count() / 1000.0f;
    }

    void Renderer::onResize(uint32_t width, uint32_t height)
    {
        m_PendingWidth = width;
        m_PendingHeight = height;
        m_ShouldResize = true;
    }

    void Renderer::createRenderTextures(uint32_t width, uint32_t height)
    {
        // Create textures
        m_MsaaRenderTarget = m_Device.createTexture(
            {
                .format = vk::Format::eR16G16B16A16Sfloat,
                .dimensions = {width, height, 1},
                .samples = m_MsaaSamples,
                .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransientAttachment,
                .memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal,
            },
            &m_GraphicsQueue, "Msaa Render Target"
        );
        m_RenderTarget = m_Device.createTexture(
            {
                .format = vk::Format::eR16G16B16A16Sfloat,
                .dimensions = {width, height, 1},
                .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc |
                         vk::ImageUsageFlagBits::eSampled,
                .memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal,
            },
            &m_GraphicsQueue,
            "RenderTarget"
        );
        m_DepthBuffer = m_Device.createTexture(
            {
                .format = vk::Format::eD32Sfloat,
                .dimensions = {width, height, 1},
                .samples = m_MsaaSamples,
                .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransientAttachment,
                .memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal,
            },
            &m_GraphicsQueue,
            "DepthBuffer"
        );
    }

    fg::RenderPassDesc Renderer::buildGeometryPass(const DrawContext& dctx, EngineStats& stats)
    {
        vk::ClearValue clearColor{vk::ClearColorValue{0.0f, 0.0f, 0.0f, 1.0f}};
        vk::ClearValue clearDepth{vk::ClearDepthStencilValue{0.0f, 0}};

        return fg::RenderPassDesc{
            .type = fg::PassType::Graphics,
            .colorWrites = {m_MsaaRenderTarget, m_RenderTarget},
            .depthWrite = m_DepthBuffer,

            .colorAttachments = {
                fg::AttachmentDesc{
                    .texture = m_MsaaRenderTarget,
                    .loadOp = vk::AttachmentLoadOp::eClear,
                    .storeOp = vk::AttachmentStoreOp::eStore,
                    .clearValue = clearColor,
                    .resolveImage = m_RenderTarget,
                    .resolveMode = vk::ResolveModeFlagBits::eAverage,
                }
            },
            .depthAttachment = fg::AttachmentDesc{
                .texture = m_DepthBuffer,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
                .clearValue = clearDepth,
            },

            .execute = [&](CommandBuffer& cmd) {
                cmd.bindDescriptorSetsBindless(vk::PipelineBindPoint::eGraphics);

                auto drawExtent = m_Device.getImage(m_MsaaRenderTarget)->extent_;
                vk::Viewport viewport{
                    0, static_cast<float>(drawExtent.height),
                    static_cast<float>(drawExtent.width), -1.0f * static_cast<float>(drawExtent.height),
                    0.0f, 1.0f
                };

                cmd.setViewport(std::move(viewport));
                cmd.setScissor(vk::Rect2D{0, 0, drawExtent.width, drawExtent.height});

                stats.drawCalls = 0;
                stats.triangleCount = 0;

                BufferHandle lastIndexBuffer;

                auto draw = [&](const RenderObject& drawObj) {
                    if (drawObj.indexBuffer != lastIndexBuffer)
                    {
                        lastIndexBuffer = drawObj.indexBuffer;
                        cmd.bindIndexBuffer(drawObj.indexBuffer, 0, vk::IndexType::eUint32);
                    }

                    GraphicsPushConstants pc{
                        .worldMatrix = drawObj.transform,
                        .vertexBufferDeviceAddress = m_Device.getBuffer(drawObj.vertexBuffer)->deviceAddress_,
                        .sceneDataBDA = m_Device.getBuffer(dctx.sceneDataBuffer)->deviceAddress_,
                        .materialBDA = m_Device.getBuffer(drawObj.material->resources.materialConstantsBuffer)->
                        deviceAddress_,
                        .colorTextureIndex = drawObj.material->resources.colorImage.index(),
                        .metallicRoughnessTextureIndex = drawObj.material->resources.metallicRoughnessImage.index(),
                        .samplerIndex = drawObj.material->resources.colorSampler.index(),
                    };
                    cmd.pushConstants(drawObj.material->pipeline->pipelineLayout, vk::ShaderStageFlagBits::eAllGraphics,
                                      0,
                                      sizeof(pc), &pc);

                    cmd.drawIndexed(drawObj.indexCount, 1, drawObj.firstIndex, 0, 0);

                    stats.drawCalls++;
                    stats.triangleCount += drawObj.indexCount / 3;
                };

                auto* d = &m_Device;
                {
                    ENGER_PROFILE_ZONENC("Opaque Surfaces", ENGER_PROFILE_COLOR_DRAW);
                    ENGER_PROFILE_GPU_ZONE("Opaque Surfaces", d, cmd.get(), ENGER_PROFILE_COLOR_DRAW);
                    if (!dctx.opaqueSurfaces.empty())
                        cmd.bindGraphicsPipeline(dctx.opaqueSurfaces[0].material->pipeline->pipeline);
                    for (auto& r : dctx.opaqueSurfaces)
                    {
                        draw(r);
                    }
                }
                {
                    ENGER_PROFILE_ZONENC("Opaque (Unlit) Surfaces", ENGER_PROFILE_COLOR_DRAW);
                    ENGER_PROFILE_GPU_ZONE("Opaque (Unlit) Surfaces", d, cmd.get(), ENGER_PROFILE_COLOR_DRAW);
                    if (!dctx.unlitSurfaces.empty())
                        cmd.bindGraphicsPipeline(dctx.unlitSurfaces[0].material->pipeline->pipeline);
                    for (auto& r : dctx.unlitSurfaces)
                    {
                        draw(r);
                    }
                }
                {
                    ENGER_PROFILE_ZONENC("Additive (Unlit) Surfaces", ENGER_PROFILE_COLOR_DRAW);
                    ENGER_PROFILE_GPU_ZONE("Additive (Unlit) Surfaces", d, cmd.get(), ENGER_PROFILE_COLOR_DRAW);
                    if (!dctx.additiveSurfaces.empty())
                    {
                        ENGER_PROFILE_ZONENC("Pipeline Bind", ENGER_PROFILE_COLOR_DRAW);
                        ENGER_PROFILE_GPU_ZONE("Pipeline Bind", d, cmd.get(), ENGER_PROFILE_COLOR_DRAW);
                        cmd.bindGraphicsPipeline(dctx.additiveSurfaces[0].material->pipeline->pipeline);
                    }
                    {
                        ENGER_PROFILE_ZONENC("Draw Call", ENGER_PROFILE_COLOR_DRAW);
                        ENGER_PROFILE_GPU_ZONE("Draw Call", d, cmd.get(), ENGER_PROFILE_COLOR_DRAW);
                        for (auto& r : dctx.additiveSurfaces)
                        {
                            draw(r);
                        }
                    }
                }
                {
                    ENGER_PROFILE_ZONENC("Transparent Surfaces", ENGER_PROFILE_COLOR_DRAW);
                    ENGER_PROFILE_GPU_ZONE("Transparent Surfaces", d, cmd.get(), ENGER_PROFILE_COLOR_DRAW);
                    if (!dctx.transparentSurfaces.empty())
                        cmd.bindGraphicsPipeline(dctx.transparentSurfaces[0].material->pipeline->pipeline);
                    for (const RenderObject& drawObj : dctx.transparentSurfaces)
                    {
                        draw(drawObj);
                    }
                }
            },

            .name = "Geometry"
        };
    }

    fg::RenderPassDesc Renderer::buildGridPass(const DrawContext& dctx, EngineStats& stats)
    {
        return fg::RenderPassDesc{
            .type = fg::PassType::Graphics,

            .colorWrites = {m_MsaaRenderTarget, m_RenderTarget},
            .depthWrite = m_DepthBuffer,

            .colorAttachments = {
                fg::AttachmentDesc{
                    .texture = m_MsaaRenderTarget,
                    .loadOp = vk::AttachmentLoadOp::eLoad,
                    .storeOp = vk::AttachmentStoreOp::eDontCare,
                    .resolveImage = m_RenderTarget,
                    .resolveMode = vk::ResolveModeFlagBits::eAverage,
            }},
            .depthAttachment = fg::AttachmentDesc{
                .texture = m_DepthBuffer,
                .loadOp = vk::AttachmentLoadOp::eLoad,
                .storeOp = vk::AttachmentStoreOp::eDontCare,
            },

            .execute = [&](CommandBuffer& cmd) {
                cmd.bindGraphicsPipeline(m_GridPipeline);
                GridPushConstants gpc{
                    .mvp = dctx.viewProj,
                    .camPos = glm::vec4(dctx.cameraPos, 1.0f),
                    .origin = glm::vec4{0.0f},
                };
                cmd.pushConstants(m_GridPipelineLayout, vk::ShaderStageFlagBits::eAllGraphics, 0, sizeof(gpc), &gpc);
                cmd.draw(6, 1, 0, 0);
                stats.drawCalls++;
                stats.triangleCount += 2;
            },

            .name = "Grid"
        };
    }

    fg::RenderPassDesc Renderer::buildTonemapperPass(TextureHandle swapchainHandle)
    {
        TonemapperPushConstants tpc{
            .srcTexIndex = m_RenderTarget.index(),
            .samplerIndex = m_Device.defaultNullSampler().index(),
        };

        std::array<std::byte, 128> pc;
        EASSERT(pc.size() >= sizeof(tpc));
        std::memcpy(pc.data(), &tpc, sizeof(tpc));

        return fg::MakePostProcessPass(
            fg::PostProcessPassDesc{
                .name = "Tonemap",
                .input = m_RenderTarget,
                .output = swapchainHandle,

                .graphicsPipeline = m_TonemapperPipeline,
                .pipelineLayout = m_TonemapperPipelineLayout,
                .pushConstantsSize = sizeof(TonemapperPushConstants),
                .pushConstantsData = std::move(pc),
            }, m_Device
        );
    }
}
