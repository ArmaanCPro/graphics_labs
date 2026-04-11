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

        file.read(reinterpret_cast<char*>(data.data()), size);
        file.close();

        return data;
    }

    Renderer::Renderer(Device &device, SwapChain& swapchain)
        : m_Device(device), m_SwapChain(swapchain), m_GraphicsQueue(device.graphicsQueue())
    {
        vk::SemaphoreCreateInfo semaphoreCI{};

        auto commandPoolsVec = m_Device.createUniqueCommandPools(CommandPoolFlags::ResetCommandBuffer,
            m_GraphicsQueue.familyIndex(), FRAMES_IN_FLIGHT, "FrameCommandPools");

        std::ranges::move(commandPoolsVec, m_CommandPools.begin());

        for (auto i = 0; i < FRAMES_IN_FLIGHT; ++i)
        {
            m_CommandBuffers[i] = m_Device.allocateCommandBuffer(m_CommandPools[i],
                CommandBufferLevel::Primary, "FrameCommandBuffer" + std::to_string(i));

            m_ImageAvailableSemaphores[i] = vkCheck(device.device().createSemaphoreUnique(semaphoreCI));
            setDebugName(m_Device.device(), *m_ImageAvailableSemaphores[i], "FrameImageAvailableSemaphore" + std::to_string(i));

        }

        for (uint32_t i = 0; i < m_SwapChain.numSwapChainImages(); ++i)
        {
            m_RenderFinishedSemaphores.push_back(vkCheck(device.device().createSemaphoreUnique(semaphoreCI)));
            setDebugName(m_Device.device(), *m_RenderFinishedSemaphores[i], "FrameRenderFinishedSemaphore" + std::to_string(i));
        }

        std::array<DescriptorAllocator::PoolSizeRatio, 1> sizes = {
            { vk::DescriptorType::eStorageImage, 1.0f }
        };
        m_DescriptorAllocator.initPool(m_Device.device(), 10, sizes);

        std::array<uint32_t, 1> bindIndices = { 0 };
        std::array<vk::DescriptorType, 1> types = { vk::DescriptorType::eStorageImage };
        m_RenderTargetDescriptorLayout = m_Device.createDescriptorSetLayout({
                                                                                .bindIndices = bindIndices,
                                                                                .types = types,
                                                                                .shaderStages = vk::ShaderStageFlagBits::eCompute,
                                                                            }, &m_GraphicsQueue, "RenderTargetDescriptorSetLayout");

        m_RenderTarget = device.createTexture({ m_SwapChain.swapChainExtent().width, m_SwapChain.swapChainExtent().height, 1 },
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

        std::array<DescriptorSetLayoutHandle, 1> setLayouts = { m_RenderTargetDescriptorLayout };
        m_GradientPipelineLayout = m_Device.createPipelineLayout({
                                                                     .descriptorLayouts = setLayouts,
                                                                 }, &m_GraphicsQueue, "GradientPipelineLayout");

        m_GradientPipeline = m_Device.createComputePipeline(ComputePipelineDesc{
                                                                .shaderModule = shaderModule,
                                                                .pipelineLayout = m_GradientPipelineLayout,
                                                            }, &m_GraphicsQueue, "GradientPipeline");
    }

    Renderer::~Renderer()
    {
        m_DescriptorAllocator.destroyPool(m_Device.device());
    }

    void Renderer::drawFrame()
    {
        m_GraphicsQueue.flushDeletionQueue();

        uint32_t swapchainImageIndex = 0;
        vkCheck(m_Device.device().acquireNextImageKHR(m_SwapChain.swapChain(),
            std::numeric_limits<uint64_t>::max(), *m_ImageAvailableSemaphores[m_CurrentFrame],
            nullptr, &swapchainImageIndex));

        CommandBuffer cmd = m_CommandBuffers[m_CurrentFrame];

        cmd.reset();

        cmd.begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        cmd.bindComputePipeline(m_GradientPipeline);

        std::array<vk::DescriptorSet, 1> descriptorSets = { m_RenderTargetDescriptor };
        cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_GradientPipelineLayout, 0, descriptorSets);

        cmd.transitionImage(m_RenderTarget, vk::ImageLayout::eUndefined, vk::ImageLayout::eGeneral);

        vk::ClearColorValue clearValue = vk::ClearColorValue{0.0f, 0.0f, 0.0f, 1.0f};

        cmd.clearColorImage(m_RenderTarget, clearValue, vk::ImageAspectFlagBits::eColor);

        cmd.transitionImage(m_RenderTarget, vk::ImageLayout::eGeneral, vk::ImageLayout::eGeneral);

        cmd.dispatch(static_cast<uint32_t>(std::ceil(m_SwapChain.swapChainExtent().width / 16.0f)),
            static_cast<uint32_t>(std::ceil(m_SwapChain.swapChainExtent().height / 16.0f)), 1);

        cmd.transitionImage(m_RenderTarget, vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferSrcOptimal);
        cmd.transitionImage(m_SwapChain.swapChainImageHandle(swapchainImageIndex),
            vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

        cmd.blitImage(m_RenderTarget, m_SwapChain.swapChainImageHandle(swapchainImageIndex));

        cmd.transitionImage(m_SwapChain.swapChainImageHandle(swapchainImageIndex),
            vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::ePresentSrcKHR);

        cmd.end();

        QueueSubmitBuilder submission{};
        submission.waitBinary(*m_ImageAvailableSemaphores[m_CurrentFrame], vk::PipelineStageFlagBits2::eColorAttachmentOutput);
        submission.signalBinary(*m_RenderFinishedSemaphores[swapchainImageIndex], vk::PipelineStageFlagBits2::eColorAttachmentOutput);
        submission.signalTimeline(m_GraphicsQueue.timelineSemaphore(), m_FrameNumber + 1, vk::PipelineStageFlagBits2::eAllGraphics);
        submission.addCmd(cmd);

        m_LastFrameSubmits[m_CurrentFrame] = m_GraphicsQueue.submit(submission.build());

        std::array<vk::Semaphore, 1> presentWaitSemaphores = { *m_RenderFinishedSemaphores[swapchainImageIndex] };
        m_SwapChain.present(presentWaitSemaphores, swapchainImageIndex,
            m_Device.graphicsQueue().queue());


        m_GraphicsQueue.wait(m_LastFrameSubmits[m_CurrentFrame]);
        m_FrameNumber++;
        m_CurrentFrame = (m_CurrentFrame + 1) % FRAMES_IN_FLIGHT;
    }
}
