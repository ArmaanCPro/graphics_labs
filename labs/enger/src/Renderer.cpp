#include "Renderer.h"

#include <expected>
#include <fstream>
#include <filesystem>

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

        file.read(reinterpret_cast<char *>(data.data()), size);
        file.close();

        return data;
    }

    Renderer::Renderer(Instance &instance, Device &device, SwapChain &swapchain, GLFWwindow *window) :
        m_Device(device),
        m_SwapChain(swapchain),
        m_GraphicsQueue(device.graphicsQueue()),
        m_ImguiLayer(instance, device, window, swapchain)
    {
        vk::SemaphoreCreateInfo semaphoreCI{};

        auto commandPoolsVec = m_Device.createUniqueCommandPools(CommandPoolFlags::ResetCommandBuffer,
                                                                 m_GraphicsQueue.familyIndex(), FRAMES_IN_FLIGHT,
                                                                 "FrameCommandPools");

        std::ranges::move(commandPoolsVec, m_CommandPools.begin());

        for (auto i = 0; i < FRAMES_IN_FLIGHT; ++i)
        {
            m_CommandBuffers[i] = m_Device.allocateCommandBuffer(m_CommandPools[i],
                                                                 CommandBufferLevel::Primary,
                                                                 "FrameCommandBuffer" + std::to_string(i));

            m_ImageAvailableSemaphores[i] = vkCheck(device.device().createSemaphoreUnique(semaphoreCI));
            setDebugName(m_Device.device(), *m_ImageAvailableSemaphores[i],
                         "FrameImageAvailableSemaphore" + std::to_string(i));

        }

        for (uint32_t i = 0; i < m_SwapChain.numSwapChainImages(); ++i)
        {
            m_RenderFinishedSemaphores.push_back(vkCheck(device.device().createSemaphoreUnique(semaphoreCI)));
            setDebugName(m_Device.device(), *m_RenderFinishedSemaphores[i],
                         "FrameRenderFinishedSemaphore" + std::to_string(i));
        }

        std::array<DescriptorAllocator::PoolSizeRatio, 1> sizes = {
            {vk::DescriptorType::eStorageImage, 1.0f}
        };
        m_DescriptorAllocator.initPool(m_Device.device(), 10, sizes);

        std::array<uint32_t, 1> bindIndices = {0};
        std::array<vk::DescriptorType, 1> types = {vk::DescriptorType::eStorageImage};
        m_RenderTargetDescriptorLayout = m_Device.createDescriptorSetLayout({
                                                                                .bindIndices = bindIndices,
                                                                                .types = types,
                                                                                .shaderStages =
                                                                                vk::ShaderStageFlagBits::eCompute,
                                                                            }, &m_GraphicsQueue,
                                                                            "RenderTargetDescriptorSetLayout");

        m_RenderTarget = device.createTexture(
            {m_SwapChain.swapChainExtent().width, m_SwapChain.swapChainExtent().height, 1},
            vk::Format::eR16G16B16A16Sfloat,
            vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst |
            vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eColorAttachment,
            &m_GraphicsQueue, "RenderTarget");

        m_RenderTargetDescriptor = m_DescriptorAllocator.allocate(m_Device,
                                                                  m_RenderTargetDescriptorLayout
            );

        // TODO very temporary, introduce descriptor writer helper later
        vk::DescriptorImageInfo imgInfo{
            .imageView = m_Device.getImage(m_RenderTarget)->view_,
            .imageLayout = vk::ImageLayout::eGeneral,
        };
        vk::WriteDescriptorSet writeInfo{
            .dstSet = m_RenderTargetDescriptor,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eStorageImage,
            .pImageInfo = &imgInfo,
        };
        m_Device.device().updateDescriptorSets(1, &writeInfo, 0, nullptr);

        // Load shader from file
        auto shaderData = loadSpirvFromFile("shaders/gradient.spv");
        if (!shaderData.has_value())
        {
            std::cerr << "Failed to load shader: shaders/gradient.spv" << std::endl;
            std::terminate();
        }

        auto shaderModule = m_Device.createShaderModule(shaderData.value(), &m_GraphicsQueue, "GradientShaderModule");

        std::array<DescriptorSetLayoutHandle, 1> setLayouts = {m_RenderTargetDescriptorLayout};
        std::array<PushConstantsInfo, 1> pushConstantRanges = {
            PushConstantsInfo{.offset = 0, .size = sizeof(ComputePushConstants), .stages = vk::ShaderStageFlagBits::eCompute}};
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
        m_TrianglePipelineLayout = m_Device.createPipelineLayout({}, &m_GraphicsQueue, "TrianglePipelineLayout");
        m_TrianglePipeline = m_Device.createGraphicsPipeline(GraphicsPipelineDesc{
            .pipelineLayout = m_TrianglePipelineLayout,
            .vertexShaderModule = triShaderModule,
            .fragmentShaderModule = triShaderModule,
            .colorAttachments = { ColorAttachment{ .format = m_Device.getImage(m_RenderTarget)->format_ } },
            .colorAttachmentCount = 1,
        }, &m_GraphicsQueue, "TrianglePipeline");
    }

    Renderer::~Renderer()
    {
        m_DescriptorAllocator.destroyPool(m_Device.device());
        m_GraphicsQueue.waitIdle();
        m_GraphicsQueue.forceDeletionQueueFlush();
        vkCheck(m_Device.device().waitIdle());
    }

    void Renderer::drawFrame()
    {
        m_GraphicsQueue.wait(m_LastFrameSubmits[m_CurrentFrame]);
        m_GraphicsQueue.flushDeletionQueue();

        uint32_t swapchainImageIndex = 0;
        vkCheck(m_Device.device().acquireNextImageKHR(m_SwapChain.swapChain(),
                                                      std::numeric_limits<uint64_t>::max(),
                                                      *m_ImageAvailableSemaphores[m_CurrentFrame],
                                                      nullptr, &swapchainImageIndex));

        CommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];

        cmd.reset();

        cmd.begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        // Compute Drawing
        cmd.bindComputePipeline(m_GradientPipeline);

        std::array<vk::DescriptorSet, 1> descriptorSets = {m_RenderTargetDescriptor};
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_GradientPipelineLayout, 0, descriptorSets);

        ComputePushConstants pc{};
        pc.data1 = {1, 0, 0, 1};
        pc.data2 = {0, 0, 1, 1};

        cmd.pushConstants(m_GradientPipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(ComputePushConstants), &pc);

        cmd.transitionImage(m_RenderTarget, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

        cmd.dispatch(static_cast<uint32_t>(std::ceil(m_SwapChain.swapChainExtent().width / 16.0f)),
                     static_cast<uint32_t>(std::ceil(m_SwapChain.swapChainExtent().height / 16.0f)), 1);

        cmd.transitionImage(m_RenderTarget, vk::ImageLayout::eGeneral, vk::ImageLayout::eColorAttachmentOptimal);

        // Geometry Drawing
        vk::RenderingAttachmentInfo colorAttachmentInfo{
            .imageView = m_Device.getImage(m_RenderTarget)->view_,
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eLoad,
            .storeOp = vk::AttachmentStoreOp::eStore,
        };
        auto drawExtent = m_Device.getImage(m_RenderTarget)->extent_;
        vk::RenderingInfo renderingInfo{
            .renderArea = vk::Rect2D{0, 0, drawExtent.width, drawExtent.height},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentInfo,
        };
        cmd.beginRendering(renderingInfo);
        cmd.bindGraphicsPipeline(m_TrianglePipeline);

        // dynamic state
        vk::Viewport viewport{0, 0, static_cast<float>(drawExtent.width), static_cast<float>(drawExtent.height), 0.0f, 1.0f};
        vk::Rect2D scissor{0, 0, drawExtent.width, drawExtent.height};
        cmd.setViewport(viewport);
        cmd.setScissor(scissor);

        cmd.draw(3, 1, 0, 0);
        cmd.endRendering();

        // transition to blit
        cmd.transitionImage(m_RenderTarget, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal);
        cmd.transitionImage(m_SwapChain.swapChainImageHandle(swapchainImageIndex),
                            vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

        cmd.blitImage(m_RenderTarget, m_SwapChain.swapChainImageHandle(swapchainImageIndex));

        // transition to swapchain drawing for ImGui
        cmd.transitionImage(m_SwapChain.swapChainImageHandle(swapchainImageIndex),
                            vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eColorAttachmentOptimal);

        m_ImguiLayer.draw(cmd.get(), m_SwapChain.swapChainImageView(swapchainImageIndex));

        // transition to swapchain present
        cmd.transitionImage(m_SwapChain.swapChainImageHandle(swapchainImageIndex),
                            vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR);

        cmd.end();

        QueueSubmitBuilder submission{};
        submission.waitBinary(*m_ImageAvailableSemaphores[m_CurrentFrame],
                              vk::PipelineStageFlagBits2::eColorAttachmentOutput);
        submission.signalBinary(*m_RenderFinishedSemaphores[swapchainImageIndex],
                                vk::PipelineStageFlagBits2::eColorAttachmentOutput);
        submission.addCmd(cmd);

        m_LastFrameSubmits[m_CurrentFrame] = m_GraphicsQueue.submit(submission.build());

        std::array<vk::Semaphore, 1> presentWaitSemaphores = {*m_RenderFinishedSemaphores[swapchainImageIndex]};
        m_SwapChain.present(presentWaitSemaphores, swapchainImageIndex,
                            m_Device.graphicsQueue().queue());

        m_CurrentFrame = (m_CurrentFrame + 1) % FRAMES_IN_FLIGHT;
    }

    GPUMeshBuffers Renderer::uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices) const
    {
        const size_t vbSize = sizeof(Vertex) * vertices.size();
        const size_t ibSize = sizeof(uint32_t) * indices.size();

        GPUMeshBuffers surface;

        surface.vertexBuffer = m_Device.createBuffer(
            vbSize,
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            &m_GraphicsQueue,
            "VertexBuffer");
        surface.indexBuffer = m_Device.createBuffer(
            ibSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            &m_GraphicsQueue,
            "IndexBuffer");

        auto staging = m_Device.createBuffer(
            vbSize + ibSize, vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
            &m_GraphicsQueue,
            "StagingBuffer"
        );

        auto* stagingBuffer = m_Device.getBuffer(staging);

        void* data = stagingBuffer->mappedMemory_;

        memcpy(data, vertices.data(), vbSize);

        return {};
    }
}
