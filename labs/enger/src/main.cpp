#include <array>
#include <span>
#include <thread>

#include "vulkan/vk.h"

#include <GLFW/glfw3.h>

#include "GlfwWindow.h"
#include "Renderer.h"
#include "vulkan/Device.h"
#include "vulkan/Instance.h"
#include "vulkan/Surface.h"
#include "vulkan/SwapChain.h"

#include "Framing.h"
#include "vulkan/QueueSubmitBuilder.h"

constexpr auto WIDTH = 800;
constexpr auto HEIGHT = 600;

int main()
{
    bool shouldRender = true;
    enger::GlfwWindow window{WIDTH, HEIGHT, "Enger"};
    window.setResizeCallback([&](uint32_t width, uint32_t height) {
        if (width == 0 || height == 0)
        {
            shouldRender = false;
        }
        else
        {
            shouldRender = true;
        }
    });

    std::vector<const char *> instanceExtensions;
#ifndef NDEBUG
    instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    auto glfwExtensions = window.requiredInstanceExtensions();
    instanceExtensions.insert(instanceExtensions.end(), glfwExtensions.begin(), glfwExtensions.end());

    enger::Instance instance{instanceExtensions};

    // maybe: don't make it vulkan specific, instead maybe map some generic extension names to vulkan specific
    std::array requiredDeviceExtensions = {
        vk::KHRSwapchainExtensionName,
    };

    auto surface = enger::Surface{window, instance.instance()};

    auto device = enger::Device{instance.instance(), surface.surface(), requiredDeviceExtensions};

    auto swapchain = enger::SwapChain{device, surface.surface(), window,
                                      vk::PresentModeKHR::eMailbox};

    enger::Renderer renderer{device, swapchain};
    enger::ImguiLayer imguiLayer{instance, device, window, swapchain};

    auto& graphicsQueue = device.graphicsQueue();
    std::array<enger::SubmitHandle, enger::framing::FRAMES_IN_FLIGHT> lastFrameSubmits = {0, 0};
    std::array<enger::UniqueCommandPool, enger::framing::FRAMES_IN_FLIGHT> m_CommandPools;
    std::array<enger::CommandBuffer, enger::framing::FRAMES_IN_FLIGHT> m_CommandBuffers;

    std::array<vk::UniqueSemaphore, enger::framing::FRAMES_IN_FLIGHT> m_ImageAvailableSemaphores;
    std::vector<vk::UniqueSemaphore> m_RenderFinishedSemaphores;
    m_RenderFinishedSemaphores.reserve(swapchain.numSwapChainImages());

    uint32_t currentFrame = 0;

    std::array<enger::framing::IFrameLayer*, 2> layers = {&renderer, &imguiLayer};

    vk::SemaphoreCreateInfo semaphoreCI{};

    auto commandPoolsVec = device.createUniqueCommandPools(enger::CommandPoolFlags::ResetCommandBuffer,
                                                             graphicsQueue.familyIndex(), enger::framing::FRAMES_IN_FLIGHT,
                                                             "FrameCommandPools");

    std::ranges::move(commandPoolsVec, m_CommandPools.begin());

    for (auto i = 0; i < enger::framing::FRAMES_IN_FLIGHT; ++i)
    {
        m_CommandBuffers[i] = device.allocateCommandBuffer(m_CommandPools[i],
                                                             enger::CommandBufferLevel::Primary,
                                                             "FrameCommandBuffer" + std::to_string(i));

        m_ImageAvailableSemaphores[i] = enger::vkCheck(device.device().createSemaphoreUnique(semaphoreCI));
        enger::setDebugName(device.device(), *m_ImageAvailableSemaphores[i],
                            "FrameImageAvailableSemaphore" + std::to_string(i));
    }

    for (uint32_t i = 0; i < swapchain.numSwapChainImages(); ++i)
    {
        m_RenderFinishedSemaphores.push_back(enger::vkCheck(device.device().createSemaphoreUnique(semaphoreCI)));
        enger::setDebugName(device.device(), *m_RenderFinishedSemaphores[i],
                     "FrameRenderFinishedSemaphore" + std::to_string(i));
    }

    while (!window.shouldClose())
    {
        window.poll();

        if (!shouldRender)
        {
            // throttle
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        graphicsQueue.wait(lastFrameSubmits[currentFrame]);
        graphicsQueue.flushDeletionQueue();

        uint32_t swapchainImageIndex = 0;
        auto acquireResult = device.device().acquireNextImageKHR(
            swapchain.swapChain(),
            std::numeric_limits<uint64_t>::max(),
            *m_ImageAvailableSemaphores[currentFrame],
            nullptr, &swapchainImageIndex
        );

        if (acquireResult == vk::Result::eErrorOutOfDateKHR)
        {
            // recreate
            continue;
        }

        enger::CommandBuffer cmd = m_CommandBuffers[currentFrame];
        cmd.reset();
        cmd.begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        enger::framing::FrameContext frameContext{
            .cmd = cmd,
            .swapchainImageIndex = swapchainImageIndex,
            .swapchainImageHandle = swapchain.swapChainImageHandle(swapchainImageIndex),
            .swapchainExtent = swapchain.swapChainExtent(),
            .frameIndex = currentFrame,
        };

        for (auto* layer : layers)
        {
            layer->draw(frameContext);
        }

        // ImGui layer, the final layer, always leaves the swapchain in Color Attachment layout
        cmd.transitionImage(frameContext.swapchainImageHandle,
            vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR);

        cmd.end();

        enger::QueueSubmitBuilder submission{};
        submission.waitBinary(*m_ImageAvailableSemaphores[currentFrame],
                              vk::PipelineStageFlagBits2::eColorAttachmentOutput);
        submission.signalBinary(*m_RenderFinishedSemaphores[swapchainImageIndex],
                                vk::PipelineStageFlagBits2::eColorAttachmentOutput);
        submission.addCmd(cmd);

        lastFrameSubmits[currentFrame] = graphicsQueue.submit(submission.build());
        auto presentResult = swapchain.present(
            {{*m_RenderFinishedSemaphores[swapchainImageIndex]}},
            swapchainImageIndex,
            graphicsQueue
        );

        if (presentResult == vk::Result::eErrorOutOfDateKHR || presentResult == vk::Result::eSuboptimalKHR)
        {
            // recreate
        }

        currentFrame = (currentFrame + 1) % enger::framing::FRAMES_IN_FLIGHT;
    }

    enger::vkCheck(device.device().waitIdle());
}
