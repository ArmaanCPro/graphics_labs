#include "Renderer.h"

#include <expected>
#include <fstream>
#include <filesystem>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "vulkan/QueueSubmitBuilder.h"

namespace enger
{
    std::expected<std::vector<uint32_t>, std::string> loadSpirvFromFile(std::filesystem::path path)
    {
        std::ifstream file(path, std::ios::ate | std::ios::binary);

        if (!file)
        {
            return std::unexpected("Failed to open file");
        }

        const auto size = file.tellg();
        std::vector<uint32_t> data(size / sizeof(uint32_t));
        file.seekg(0);

        file.read(reinterpret_cast<char*>(data.data()), size);
        file.close();

        return data;
    }

    Renderer::Renderer(Device& device, SwapChain& swapchain) :
        m_Device(device),
        m_SwapChain(swapchain),
        m_GraphicsQueue(device.graphicsQueue())
    {
        // Create textures
        m_RenderTarget = device.createTexture(
            {m_SwapChain.swapChainExtent().width, m_SwapChain.swapChainExtent().height, 1},
            vk::Format::eR16G16B16A16Sfloat,
            vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst |
            vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eColorAttachment,
            &m_GraphicsQueue, "RenderTarget"
        );

        m_DepthBuffer = device.createTexture(
            {m_SwapChain.swapChainExtent().width, m_SwapChain.swapChainExtent().height, 1},
            vk::Format::eD32Sfloat,
            vk::ImageUsageFlagBits::eDepthStencilAttachment,
            &m_GraphicsQueue, "DepthBuffer"
        );

        // Load shader from file
        auto shaderData = loadSpirvFromFile("shaders/gradient.spv");
        if (!shaderData.has_value())
        {
            std::cerr << "Failed to load shader: shaders/gradient.spv" << std::endl;
            std::terminate();
        }

        auto shaderModule = m_Device.createShaderModule(shaderData.value(), &m_GraphicsQueue, "GradientShaderModule");

        std::array<DescriptorSetLayoutHandle, 1> setLayouts = {m_Device.bindlessDescriptorSetLayout()};
        std::array<PushConstantsInfo, 1> pushConstantRanges = {
            PushConstantsInfo{
                .offset = 0, .size = sizeof(ComputePushConstants), .stages = vk::ShaderStageFlagBits::eCompute
            }
        };
        m_GradientPipelineLayout = m_Device.createPipelineLayout({
                                                                     .descriptorLayouts = setLayouts,
                                                                     .pushConstantRanges = pushConstantRanges
                                                                 }, &m_GraphicsQueue, "GradientPipelineLayout");

        m_GradientPipeline = m_Device.createComputePipeline(ComputePipelineDesc{
                                                                .pipelineLayout = m_GradientPipelineLayout,
                                                                .shaderModule = shaderModule,
                                                            }, &m_GraphicsQueue, "GradientPipeline");

        // Colored Triangle Graphics Pipeline
        std::vector<uint32_t> triShaderData = std::move(loadSpirvFromFile("shaders/colored_triangle.spv").value());
        auto triShaderModule = m_Device.createShaderModule(triShaderData, &m_GraphicsQueue, "TriangleShaderModule");

        m_GraphicsPipelineLayout = m_Device.createPipelineLayout({
                                                                     .pushConstantRanges = std::array{
                                                                         PushConstantsInfo{
                                                                             .offset = 0,
                                                                             .size = sizeof(DrawPushConstants),
                                                                             .stages = vk::ShaderStageFlagBits::eVertex,
                                                                         }
                                                                     },
                                                                 }, &m_GraphicsQueue, "TrianglePipelineLayout");
        m_GraphicsPipeline = m_Device.createGraphicsPipeline(GraphicsPipelineDesc{
                                                                 .pipelineLayout = m_GraphicsPipelineLayout,
                                                                 .vertexShaderModule = triShaderModule,
                                                                 .fragmentShaderModule = triShaderModule,
                                                                 .depthTestEnable = true,
                                                                 .depthWriteEnable = true,
                                                                 .depthCompareOp = vk::CompareOp::eGreaterOrEqual,
                                                                 .colorAttachments = {
                                                                     ColorAttachment{
                                                                         .format = m_Device.getImage(m_RenderTarget)->
                                                                         format_,
                                                                         .blendEnabled = true,
                                                                         .srcRgbBlendFactor = vk::BlendFactor::eSrcAlpha,
                                                                         .dstRgbBlendFactor = vk::BlendFactor::eOne,
                                                                         .rgbBlendOp = vk::BlendOp::eAdd,
                                                                         .srcAlphaBlendFactor = vk::BlendFactor::eOne,
                                                                         .dstAlphaBlendFactor = vk::BlendFactor::eZero,
                                                                         .alphaBlendOp = vk::BlendOp::eAdd,
                                                                     }
                                                                 },
                                                                 .colorAttachmentCount = 1,
                                                                 .depthFormat = m_Device.getImage(m_DepthBuffer)->
                                                                 format_,
                                                                 .frontFace = vk::FrontFace::eCounterClockwise,
                                                             }, &m_GraphicsQueue, "TrianglePipeline");


        auto expectedMeshes = LoadMeshes(m_Device, "assets/basicmesh.glb");
        if (!expectedMeshes)
        {
            std::cerr << "Failed to load meshes" << std::endl;
            std::terminate();
        }
        m_TestMeshes = expectedMeshes.value();
    }

    void Renderer::draw(framing::FrameContext& fctx)
    {
        auto cmd = fctx.cmd;

        // Compute Drawing
        cmd.bindComputePipeline(m_GradientPipeline);

        cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_GradientPipelineLayout, 0,
                               {{m_Device.bindlessDescriptorSet()}});

        ComputePushConstants pc{};
        pc.data1 = {1, 0, 0, 1};
        pc.data2 = {0, 0, 1, 1};
        pc.textureIndex = m_RenderTarget.index();

        cmd.pushConstants(m_GradientPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(ComputePushConstants),
                          &pc);

        cmd.transitionImage(m_RenderTarget, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

        cmd.dispatch(static_cast<uint32_t>(std::ceil(m_SwapChain.swapChainExtent().width / 16.0f)),
                     static_cast<uint32_t>(std::ceil(m_SwapChain.swapChainExtent().height / 16.0f)), 1);

        cmd.transitionImage(m_RenderTarget, vk::ImageLayout::eGeneral, vk::ImageLayout::eColorAttachmentOptimal);
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
        cmd.bindGraphicsPipeline(m_GraphicsPipeline);

        // dynamic state
        vk::Viewport viewport{
            0, static_cast<float>(drawExtent.height),
            static_cast<float>(drawExtent.width), -1.0f * static_cast<float>(drawExtent.height),
            0.0f, 1.0f
        };
        vk::Rect2D scissor{0, 0, drawExtent.width, drawExtent.height};
        cmd.setViewport(viewport);
        cmd.setScissor(scissor);

        glm::mat4 view = glm::translate(glm::mat4{1.0f}, glm::vec3{0, 0, -5});
        glm::mat4 projection = glm::perspective(glm::radians(70.0f),
                                                static_cast<float>(drawExtent.width) / static_cast<float>(drawExtent.
                                                    height), 1000.0f, 0.1f);
        DrawPushConstants pushConstants{
            .worldMatrix = projection * view,
            .vertexBufferDeviceAddress = m_Device.getBuffer(m_TestMeshes[2]->meshBuffers.vertexBuffer)->deviceAddress_,
        };
        cmd.pushConstants(m_GraphicsPipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, sizeof(DrawPushConstants),
                          &pushConstants);
        cmd.bindIndexBuffer(m_TestMeshes[2].get()->meshBuffers.indexBuffer, 0, vk::IndexType::eUint32);

        cmd.drawIndexed(m_TestMeshes[2]->surfaces[0].indexCount, 1, m_TestMeshes[2]->surfaces[0].startIndex, 0, 0);
        cmd.endRendering();

        // transition to blit
        cmd.transitionImage(m_RenderTarget, vk::ImageLayout::eColorAttachmentOptimal,
                            vk::ImageLayout::eTransferSrcOptimal);
        cmd.transitionImage(fctx.swapchainImageHandle,
                            vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

        cmd.blitImage(m_RenderTarget, fctx.swapchainImageHandle);
    }
}
