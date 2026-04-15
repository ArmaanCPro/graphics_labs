#include "Renderer.h"

#include <execution>
#include <expected>
#include <fstream>
#include <filesystem>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "vulkan/QueueSubmitBuilder.h"

#include "SceneGraph.h"

#include "MeshLoader.h"
#include "vulkan/PushConstantRanges.h"

namespace enger
{
    Renderer::Renderer(Device& device, SwapChain& swapchain) :
        m_Device(device),
        m_SwapChain(swapchain),
        m_GraphicsQueue(device.graphicsQueue())
    {
        // Create textures
        createRenderTextures(m_SwapChain.swapChainExtent().width, m_SwapChain.swapChainExtent().height);
    }

    void Renderer::render(framing::FrameContext& fctx, const DrawContext& dctx, EngineStats& stats)
    {
        if (m_ShouldResize)
        {
            createRenderTextures(m_PendingWidth, m_PendingHeight);
            m_ShouldResize = false;
        }

        auto& cmd = fctx.cmd;

        cmd.transitionImage(m_RenderTarget, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);
        cmd.transitionImage(m_DepthBuffer, vk::ImageLayout::eUndefined,
                            vk::ImageLayout::eDepthStencilAttachmentOptimal);
        cmd.transitionImage(m_MsaaRenderTarget, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);

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
        auto drawExtent = m_Device.getImage(m_MsaaRenderTarget)->extent_;
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
        vk::Rect2D scissor{0, 0, drawExtent.width, drawExtent.height};
        cmd.setViewport(viewport);
        cmd.setScissor(scissor);

        stats.drawCalls = 0;
        stats.triangleCount = 0;
        auto start = std::chrono::high_resolution_clock::now();
        
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
                .sceneDataBDA = m_Device.getBuffer(drawObj.material->resources.dataBuffer)->deviceAddress_,
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
            draw(r);
        }

        if (!dctx.transparentSurfaces.empty())
            cmd.bindGraphicsPipeline(dctx.transparentSurfaces[0].material->pipeline->pipeline);
        for (const RenderObject& drawObj : dctx.transparentSurfaces)
        {
            draw(drawObj);
        }

        cmd.endRendering();

        cmd.transitionImage(m_RenderTarget, vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal);
        cmd.transitionImage(fctx.swapchainImageHandle, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
        cmd.blitImage(m_RenderTarget, fctx.swapchainImageHandle);

        cmd.transitionImage(fctx.swapchainImageHandle, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eColorAttachmentOptimal);

        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        stats.meshDrawTime = elapsed.count() / 1000.0f;

        // MSAA Resolve handled the blit from msaa render target to render target
        // We still blit from render target to swapchain
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
                .usage = vk::ImageUsageFlagBits::eColorAttachment,
                .memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal,
            },
            &m_GraphicsQueue, "Msaa Render Target"
        );
        m_RenderTarget = m_Device.createTexture(
            {
                .format = vk::Format::eR16G16B16A16Sfloat,
                .dimensions = {width, height, 1},
                .usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
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
