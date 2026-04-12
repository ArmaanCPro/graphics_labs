#include <array>
#include <span>
#include <thread>

#include "vulkan/vk.h"

#include <GLFW/glfw3.h>

#include "Renderer.h"
#include "vulkan/Device.h"
#include "vulkan/Instance.h"
#include "vulkan/Surface.h"
#include "vulkan/SwapChain.h"

constexpr auto WIDTH = 800;
constexpr auto HEIGHT = 600;

void glfwSizeCallback(GLFWwindow* window, int width, int height)
{
    bool& shouldRender = *static_cast<bool*>(glfwGetWindowUserPointer(window));

    if (width == 0 || height == 0)
    {
        shouldRender = false;
    }
    else
    {
        shouldRender = true;
    }
}

int main()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "Enger", nullptr, nullptr);

    bool shouldRender = true;
    glfwSetWindowUserPointer(window, &shouldRender);
    glfwSetWindowSizeCallback(window, glfwSizeCallback);

    std::vector<const char *> instanceExtensions;
#ifndef NDEBUG
    instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    instanceExtensions.insert(instanceExtensions.end(), glfwExtensions, glfwExtensions + glfwExtensionCount);

    enger::Instance instance{instanceExtensions};

    // maybe: don't make it vulkan specific, instead maybe map some generic extension names to vulkan specific
    std::array requiredDeviceExtensions = {
        vk::KHRSwapchainExtensionName,
    };

    auto surface = enger::Surface{window, instance.instance()};

    auto device = enger::Device{instance.instance(), surface.surface(), requiredDeviceExtensions};

    auto swapchain = enger::SwapChain{device, surface.surface(), window,
                                      vk::PresentModeKHR::eMailbox};

    enger::Renderer renderer{instance, device, swapchain, window};

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        if (!shouldRender)
        {
            // throttle
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        renderer.drawFrame();
    }

    enger::vkCheck(device.device().waitIdle());

    glfwDestroyWindow(window);
    glfwTerminate();
}
