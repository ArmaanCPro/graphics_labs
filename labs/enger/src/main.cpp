#include <array>
#include <span>
#include <stack>
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

struct ResizeEvent
{
    uint32_t newWidth;
    uint32_t newHeight;
};

int main()
{
    enger::GlfwWindow window{WIDTH, HEIGHT, "Enger"};

    std::stack<ResizeEvent> resizeEventBus;

    window.setResizeCallback([&](uint32_t width, uint32_t height) {
        resizeEventBus.push({width, height});
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

    enger::framing::FrameOrchestrator frameOrchestrator{device, swapchain, window};

    while (!window.shouldClose())
    {
        window.poll();

        if (resizeEventBus.size() > 0)
        {
            auto event = resizeEventBus.top();
            while (resizeEventBus.size() > 0)
            {
                resizeEventBus.pop();
            }

            frameOrchestrator.onWindowResize(event.newWidth, event.newHeight);
            renderer.onResize(event.newWidth, event.newHeight);
            imguiLayer.onResize(event.newWidth, event.newHeight);
        }

        if (auto optfctx = frameOrchestrator.beginFrame())
        {
            auto fctx = std::move(*optfctx);
            renderer.draw(fctx);

            imguiLayer.beginFrame();
            // we could remove imguiLayer.draw() and put our own imgui drawing here
            imguiLayer.draw();
            imguiLayer.endFrame(fctx);

            frameOrchestrator.endFrame(fctx);

            imguiLayer.postRenderFinished();
        }
    }

    device.waitIdle();
}
