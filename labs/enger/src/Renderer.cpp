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

        auto expectedMeshes = LoadMeshes(m_Device, "assets/basicmesh.glb");
        if (!expectedMeshes)
        {
            std::cerr << "Failed to load meshes" << std::endl;
            std::terminate();
        }
        m_TestMeshes = expectedMeshes.value();

        // Default Textures
        uint32_t white = glm::packUnorm4x8(glm::vec4{1.0f, 1.0f, 1.0f, 1.0f});
        m_WhiteImage = m_Device.createTexture({
                                                  .format = vk::Format::eR8G8B8A8Unorm,
                                                  .dimensions = {1, 1, 1},
                                                  .usage = vk::ImageUsageFlagBits::eSampled |
                                                           vk::ImageUsageFlagBits::eTransferDst,
                                                  .memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                                                  .initialData = &white,
                                              }, &m_GraphicsQueue, "WhiteImage");

        uint32_t gray = glm::packUnorm4x8(glm::vec4{0.66f, 0.66f, 0.66f, 1.0f});
        m_GrayImage = m_Device.createTexture({
                                                 .format = vk::Format::eR8G8B8A8Unorm,
                                                 .dimensions = {1, 1, 1},
                                                 .usage = vk::ImageUsageFlagBits::eSampled |
                                                          vk::ImageUsageFlagBits::eTransferDst,
                                                 .memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                                                 .initialData = &gray,
                                             }, &m_GraphicsQueue, "GrayImage");

        uint32_t black = glm::packUnorm4x8(glm::vec4{0.0f, 0.0f, 0.0f, 1.0f});
        m_BlackImage = m_Device.createTexture({
                                                  .format = vk::Format::eR8G8B8A8Unorm,
                                                  .dimensions = {1, 1, 1},
                                                  .usage = vk::ImageUsageFlagBits::eSampled |
                                                           vk::ImageUsageFlagBits::eTransferDst,
                                                  .memoryProperties = vk::MemoryPropertyFlagBits::eDeviceLocal,
                                                  .initialData = &black,
                                              }, &m_GraphicsQueue, "BlackImage"); {
            // checkerboard image
            uint32_t magenta = glm::packUnorm4x8(glm::vec4{1.0f, 0.0f, 1.0f, 1.0f});
            std::array<uint32_t, 16 * 16> pixels;
            for (int x = 0; x < 16; ++x)
            {
                for (int y = 0; y < 16; ++y)
                {
                    pixels[y * 16 + x] = (x & 1) ^ (y & 1) ? magenta : black;
                }
            }
            m_ErrorCheckerboardImage = m_Device.createTexture({
                                                                  .format = vk::Format::eR8G8B8A8Unorm,
                                                                  .dimensions = {16, 16, 1},
                                                                  .usage = vk::ImageUsageFlagBits::eSampled |
                                                                           vk::ImageUsageFlagBits::eTransferDst,
                                                                  .memoryProperties =
                                                                  vk::MemoryPropertyFlagBits::eDeviceLocal,
                                                                  .initialData = pixels.data(),
                                                              }, &m_GraphicsQueue, "ErrorCheckerboardImage");

            m_DefaultSamplerLinear = m_Device.createSampler(
                vk::Filter::eLinear, vk::Filter::eLinear, &m_GraphicsQueue, "DefaultSamplerLinear"
            );

            m_DefaultSamplerNearest = m_Device.createSampler(
                vk::Filter::eNearest, vk::Filter::eNearest, &m_GraphicsQueue, "DefaultSamplerNearest"
            );
        }

        // GPU scene data
        m_GPUSceneDataBuffer = m_Device.createBuffer(
            sizeof(GPUSceneData),
            vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            &m_GraphicsQueue, "GPUSceneDataBuffer"
        );

        // MATERIALS
        m_GLTFMetallic_Roughness.buildPipelines(m_Device, m_Device.getImage(m_RenderTarget)->format_,
            m_Device.getImage(m_DepthBuffer)->format_);

        MaterialResources materialResources;
        materialResources.colorImage = m_WhiteImage;
        materialResources.colorSampler = m_DefaultSamplerNearest;
        materialResources.metallicRoughnessImage = m_WhiteImage;
        materialResources.metallicRoughnessSampler = m_DefaultSamplerLinear;

        materialResources.dataBuffer = m_GPUSceneDataBuffer;

        auto materialConstants = m_Device.createBuffer(sizeof(MaterialConstants),
            vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            &m_GraphicsQueue, "MaterialConstants"
        );
        MaterialConstants materialConstantsData{.colorFactors = glm::vec4(1), .metallicRoughnessFactors = glm::vec4(1)};
        m_Device.getBuffer(materialConstants)->bufferSubData(m_Device.allocator(), 0, sizeof(MaterialConstants),
                                                             &materialConstantsData);

        materialResources.materialConstantsBuffer = std::move(materialConstants);

        m_DefaultMaterial = m_GLTFMetallic_Roughness.writeMaterial(MaterialPass::MainColor, std::move(materialResources));

        for (auto& m : m_TestMeshes)
        {
            std::shared_ptr<MeshNode> newNode = std::make_shared<MeshNode>();
            newNode->mesh = m;

            newNode->localTransform = glm::mat4(1.0f);
            newNode->worldTransform = glm::mat4(1.0f);

            for (auto& s : newNode->mesh->surfaces)
            {
                s.material = std::move(std::make_shared<GLTFMaterial>(m_DefaultMaterial));
            }

            m_LoadedNodes[m->name] = std::move(newNode);
        }
    }

    void Renderer::draw(framing::FrameContext& fctx)
    {
        auto& cmd = fctx.cmd;

        cmd.transitionImage(m_RenderTarget, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
        cmd.transitionImage(m_DepthBuffer, vk::ImageLayout::eUndefined,
                            vk::ImageLayout::eDepthStencilAttachmentOptimal);

        // Geometry Drawing
        vk::RenderingAttachmentInfo colorAttachmentInfo{
            .imageView = m_Device.getImage(m_RenderTarget)->view_,
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eLoad,
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
        
        updateScene();
        m_Device.getBuffer(m_GPUSceneDataBuffer)->bufferSubData(m_Device.allocator(), 0, sizeof(GPUSceneData),
            &m_SceneData);


        // Currently, all render objects are opaque
        fctx.cmd.bindGraphicsPipeline(m_GLTFMetallic_Roughness.opaquePipeline.pipeline);
        // TODO right now the pipeline layout system is very unsturdy. I need a global bindless pipeline layout (meaning 1 descriptor set layout and 1 push constant range)
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_GLTFMetallic_Roughness.opaquePipeline.pipelineLayout, 0,
                    {{m_Device.bindlessDescriptorSet()}});

        for (const RenderObject& drawObj : m_MainDrawContext.opaqueSurfaces)
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
    }

    void Renderer::onResize(uint32_t width, uint32_t height)
    {
        createRenderTextures(width, height);
    }

    void Renderer::updateScene()
    {
        m_MainDrawContext.opaqueSurfaces.clear();

        m_LoadedNodes["Suzanne"]->draw(glm::mat4(1.0f), m_MainDrawContext);

        m_SceneData.view = glm::translate(glm::mat4(1.0f), glm::vec3{0, 0, -5});
        m_SceneData.proj = glm::perspective(glm::radians(70.0f),
            static_cast<float>(m_SwapChain.swapChainExtent().width) / static_cast<float>(m_SwapChain.swapChainExtent().height),
            10000.0f, 0.1f);

        m_SceneData.viewProj = m_SceneData.proj * m_SceneData.view;

        m_SceneData.ambientColor = glm::vec4(1.0f);
        m_SceneData.sunlightColor = glm::vec4(1.0f);
        m_SceneData.sunlightDirection = glm::vec4(0.0f, 1.0f, 0.5f, 1.0f);

        for (int x = -3; x < 3; x++)
        {
            glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3{0.2f});
            glm::mat4 translation = glm::translate(glm::mat4(1.0f), glm::vec3{x, 1, 0});

            m_LoadedNodes["Cube"]->draw(translation * scale, m_MainDrawContext);
        }
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
