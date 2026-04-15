#include "Renderer.h"

#include <expected>
#include <fstream>
#include <filesystem>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "vulkan/QueueSubmitBuilder.h"

#include "SceneGraph.h"

#include "MeshLoader.h"

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

    void Renderer::render(framing::FrameContext& fctx, const DrawContext& dctx)
    {
        if (m_ShouldResize)
        {
            createRenderTextures(m_PendingWidth, m_PendingHeight);
            m_ShouldResize = false;
        }

        auto& cmd = fctx.cmd;

        cmd.transitionImage(m_RenderTarget, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
        cmd.transitionImage(m_DepthBuffer, vk::ImageLayout::eUndefined,
                            vk::ImageLayout::eDepthStencilAttachmentOptimal);

        // Geometry Drawing
        vk::RenderingAttachmentInfo colorAttachmentInfo{
            .imageView = m_Device.getImage(m_RenderTarget)->view_,
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
        };
        vk::RenderingAttachmentInfo depthAttachmentInfo{
            .imageView = m_Device.getImage(m_DepthBuffer)->view_,
            .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .clearValue = vk::ClearValue{vk::ClearDepthStencilValue{0.0f, 0}},
        };
        auto drawExtent = m_Device.getImage(m_RenderTarget)->extent_;
        vk::RenderingInfo renderingInfo{
            .renderArea = vk::Rect2D{0, 0, drawExtent.width, drawExtent.height},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentInfo,
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
        
        // Currently, all render objects are opaque
        assert(dctx.opaqueSurfaces.size() > 0);
        cmd.bindGraphicsPipeline(dctx.opaqueSurfaces[0].material->pipeline->pipeline);
        // TODO right now the pipeline layout system is very unsturdy. I need a global bindless pipeline layout (meaning 1 descriptor set layout and 1 push constant range)
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, dctx.opaqueSurfaces[0].material->pipeline->pipelineLayout, 0,
                    {{m_Device.bindlessDescriptorSet()}});

        for (const RenderObject& drawObj : dctx.opaqueSurfaces)
        {
            fctx.cmd.bindIndexBuffer(drawObj.indexBuffer, 0, vk::IndexType::eUint32);

            DrawPushConstants pc{
                .worldMatrix = drawObj.transform,
                .vertexBufferDeviceAddress = m_Device.getBuffer(drawObj.vertexBuffer)->deviceAddress_,
                .sceneDataBDA = m_Device.getBuffer(drawObj.material->resources.dataBuffer)->deviceAddress_,
                .materialBDA = m_Device.getBuffer(drawObj.material->resources.materialConstantsBuffer)->deviceAddress_,
                .colorTextureIndex = drawObj.material->resources.colorImage.index(),
                .metallicRoughnessTextureIndex = drawObj.material->resources.metallicRoughnessImage.index(),
                .samplerIndex = drawObj.material->resources.colorSampler.index(),
            };
            fctx.cmd.pushConstants(drawObj.material->pipeline->pipelineLayout,
                vk::ShaderStageFlagBits::eAllGraphics, 0, sizeof(pc), &pc);

            fctx.cmd.drawIndexed(drawObj.indexCount, 1, drawObj.firstIndex, 0, 0);
        }

        cmd.endRendering();

        // transition to blit
        cmd.transitionImage(m_RenderTarget, vk::ImageLayout::eColorAttachmentOptimal,
                            vk::ImageLayout::eTransferSrcOptimal);
        cmd.transitionImage(fctx.swapchainImageHandle,
                            vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

        cmd.blitImage(m_RenderTarget, fctx.swapchainImageHandle);

        // Leave the swapchain image in a usable state for UI
        cmd.transitionImage(fctx.swapchainImageHandle, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eColorAttachmentOptimal);
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
        m_RenderTarget = m_Device.createTexture(
            {
                .format = vk::Format::eR16G16B16A16Sfloat,
                .dimensions = {width, height, 1},
                .usage = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eColorAttachment,
                .memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                .initialData = nullptr,
            },
            &m_GraphicsQueue,
            "RenderTarget"
        );
        m_DepthBuffer = m_Device.createTexture(
            {
                .format = vk::Format::eD32Sfloat,
                .dimensions = {width, height, 1},
                .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
                .memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                .initialData = nullptr,
            },
            &m_GraphicsQueue,
            "DepthBuffer"
        );
    }
}
