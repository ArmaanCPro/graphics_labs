#include "Renderer.h"

#include <execution>
#include <expected>
#include <fstream>
#include <filesystem>

#include "vulkan/QueueSubmitBuilder.h"

#include "SceneGraph.h"

#include "MeshLoader.h"
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

        // Create tonemapping pipeline (could move to a frame graph later on)
        m_TonemapperPipelineLayout = m_Device.createPipelineLayout(PipelineLayoutDesc{
            .descriptorLayouts = {{m_Device.bindlessDescriptorSetLayout()}},
            .pushConstantRanges = {{PushConstantsInfo{
                .offset = 0,
                .size = sizeof(TonemapperPushConstants),
                .stages = vk::ShaderStageFlagBits::eFragment}}},
        }, &m_GraphicsQueue, "Tonemapper Pipeline Layout");

        auto tonemapperSpirv = loadSpirvFromFile("shaders/Reinhard.spv");
        assert(tonemapperSpirv.has_value());
        auto tonemapperSM = m_Device.createShaderModule(std::move(tonemapperSpirv.value()), &m_GraphicsQueue, "Tonemapper Shader Module");

        m_TonemapperPipeline = m_Device.createGraphicsPipeline(GraphicsPipelineDesc{
            .pipelineLayout = m_TonemapperPipelineLayout,
            .vertexShaderModule = tonemapperSM,
            .fragmentShaderModule = tonemapperSM,

            .colorAttachments = {ColorAttachment{
                .format = m_SwapChain.swapChainFormat()
            }},
            .colorAttachmentCount = 1,

#ifndef NDEBUG
            .enablePipelineRobustness = true,
#endif
        }, &m_GraphicsQueue, "Tonemapper Pipeline");
    }

    void Renderer::render(framing::FrameContext& fctx, const DrawContext& dctx, EngineStats& stats)
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

        auto drawExtent = m_Device.getImage(m_MsaaRenderTarget)->extent_;
        vk::Rect2D scissor{0, 0, drawExtent.width, drawExtent.height};
        std::vector<BufferHandle> allVertexBuffers;
        std::vector<BufferHandle> allIndexBuffers;
        {
            ENGER_PROFILE_ZONENC("Geometry Pass Frame", ENGER_PROFILE_COLOR_DRAW);
            ENGER_PROFILE_GPU_ZONE("Geometry Pass", d, fctx.cmd.get(), ENGER_PROFILE_COLOR_DRAW);
            cmd.transitionImages(std::array{
                TransitionImageInfo{m_RenderTarget, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral},
                TransitionImageInfo{m_DepthBuffer, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal},
                TransitionImageInfo{m_MsaaRenderTarget, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal}
            });

            // Geometry Drawing
            vk::RenderingAttachmentInfo colorAttachmentInfo{
                .imageView = m_Device.getImage(m_MsaaRenderTarget)->view_,
                .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .resolveMode = vk::ResolveModeFlagBits::eAverage,
                .resolveImageView = m_Device.getImage(m_RenderTarget)->view_,
                .resolveImageLayout = vk::ImageLayout::eGeneral,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eDontCare,
            };
            vk::RenderingAttachmentInfo depthAttachmentInfo{
                .imageView = m_Device.getImage(m_DepthBuffer)->view_,
                .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eDontCare,
                .clearValue = vk::ClearValue{vk::ClearDepthStencilValue{0.0f, 0}},
            };
            vk::RenderingInfo renderingInfo{
                .renderArea = vk::Rect2D{0, 0, drawExtent.width, drawExtent.height},
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &colorAttachmentInfo,
                .pDepthAttachment = &depthAttachmentInfo,
            };
            cmd.beginRendering(renderingInfo);

            // dynamic state
            vk::Viewport viewport{
                0, static_cast<float>(drawExtent.height),
                static_cast<float>(drawExtent.width), -1.0f * static_cast<float>(drawExtent.height),
                0.0f, 1.0f
            };
            cmd.setViewport(viewport);
            cmd.setScissor(scissor);

            stats.drawCalls = 0;
            stats.triangleCount = 0;

            cmd.bindDescriptorSetsBindless(vk::PipelineBindPoint::eGraphics);

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
                    .materialBDA = m_Device.getBuffer(drawObj.material->resources.materialConstantsBuffer)->deviceAddress_,
                    .colorTextureIndex = drawObj.material->resources.colorImage.index(),
                    .metallicRoughnessTextureIndex = drawObj.material->resources.metallicRoughnessImage.index(),
                    .samplerIndex = drawObj.material->resources.colorSampler.index(),
                };
                cmd.pushConstants(drawObj.material->pipeline->pipelineLayout, vk::ShaderStageFlagBits::eAllGraphics, 0, sizeof(pc), &pc);

                cmd.drawIndexed(drawObj.indexCount, 1, drawObj.firstIndex, 0, 0);

                stats.drawCalls++;
                stats.triangleCount += drawObj.indexCount / 3;
            };


            if (!dctx.opaqueSurfaces.empty())
                cmd.bindGraphicsPipeline(dctx.opaqueSurfaces[0].material->pipeline->pipeline);
            for (auto& r : dctx.opaqueSurfaces)
            {
                if (m_IsFirstFrame)
                {
                    if (m_Device.transferQueue().has_value())
                    {
                        allVertexBuffers.push_back(r.vertexBuffer);
                        allIndexBuffers.push_back(r.indexBuffer);
                    }
                }
                draw(r);
            }

            if (!dctx.transparentSurfaces.empty())
                cmd.bindGraphicsPipeline(dctx.transparentSurfaces[0].material->pipeline->pipeline);
            for (const RenderObject& drawObj : dctx.transparentSurfaces)
            {
                if (m_IsFirstFrame)
                {
                    if (m_Device.transferQueue().has_value())
                    {
                        allVertexBuffers.push_back(drawObj.vertexBuffer);
                        allIndexBuffers.push_back(drawObj.indexBuffer);
                    }
                }
                draw(drawObj);
            }

            cmd.endRendering();
        }

        {
            ENGER_PROFILE_ZONENC("Tone Mapping Pass", ENGER_PROFILE_COLOR_DRAW);
            ENGER_PROFILE_GPU_ZONE("Tone Mapping Pass", d, fctx.cmd.get(), ENGER_PROFILE_COLOR_DRAW);
            cmd.transitionImages(std::array{
                TransitionImageInfo{m_RenderTarget, vk::ImageLayout::eGeneral, vk::ImageLayout::eShaderReadOnlyOptimal},
                TransitionImageInfo{fctx.swapchainImageHandle, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal},
            });

            const TonemapperPushConstants tpc = {
                .srcTexIndex = m_RenderTarget.index(),
                .samplerIndex = m_Device.defaultNullSampler().index(), // create tonemapper sampler?
            };
            vk::RenderingAttachmentInfo tonemapperAttachmentInfo{
                .imageView = m_SwapChain.swapChainImageView(fctx.swapchainImageIndex),
                .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .loadOp = vk::AttachmentLoadOp::eClear,
                .storeOp = vk::AttachmentStoreOp::eStore,
            };
            vk::RenderingInfo tonemapperRenderingInfo{
                .renderArea = vk::Rect2D{0, 0, fctx.swapchainExtent.width, fctx.swapchainExtent.height},
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &tonemapperAttachmentInfo,
            };
            vk::Viewport tmViewport{
                .x = 0.0f,
                .y = 0.0f,
                .width = static_cast<float>(fctx.swapchainExtent.width),
                .height = static_cast<float>(fctx.swapchainExtent.height),
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
            };

            cmd.beginRendering(tonemapperRenderingInfo);
            cmd.setViewport(tmViewport);
            cmd.setScissor(scissor);
            cmd.bindGraphicsPipeline(m_TonemapperPipeline);
            cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_TonemapperPipelineLayout, 0, {{m_Device.bindlessDescriptorSet()}});

            cmd.pushConstants(m_TonemapperPipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, sizeof(tpc), &tpc);
            cmd.draw(3, 1, 0, 0);
            cmd.endRendering();
        }

        if (m_IsFirstFrame && m_Device.transferQueue().has_value())
        {
            ENGER_PROFILE_GPU_ZONE("Transfer Vert/Index Transfer Queue to Graphics Queue", d, fctx.cmd.get(), ENGER_PROFILE_COLOR_BARRIER);
            auto sbv = cmd.bufferBarrier({
                allVertexBuffers,
                vk::AccessFlagBits2::eTransferWrite,
                vk::AccessFlagBits2::eShaderStorageRead,
                vk::PipelineStageFlagBits2::eTransfer,
                vk::PipelineStageFlagBits2::eVertexShader,
                m_Device.transferQueue().value(),
                m_Device.graphicsQueue(),
            });
            allVertexBuffers.clear();
            auto sbi = cmd.bufferBarrier({
                allIndexBuffers,
                vk::AccessFlagBits2::eTransferWrite,
                vk::AccessFlagBits2::eIndexRead,
                vk::PipelineStageFlagBits2::eTransfer,
                vk::PipelineStageFlagBits2::eIndexInput,
                m_Device.transferQueue().value(),
                m_Device.graphicsQueue(),
            });
            fctx.desiredWaits.push_back(sbv);
            fctx.desiredWaits.push_back(sbi);
        }
        m_IsFirstFrame = false;

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
                .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled,
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
}
