#include <array>
#include <span>

#include "vulkan/vk.h"

#include <GLFW/glfw3.h>

#include "vulkan/Device.h"
#include "vulkan/Instance.h"
#include "vulkan/Surface.h"
#include "vulkan/SwapChain.h"

constexpr auto WIDTH = 800;
constexpr auto HEIGHT = 600;

int main()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Enger", nullptr, nullptr);

    std::vector<const char*> instanceExtensions;
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

    auto swapchain = enger::SwapChain{device.physicalDevice(), device.device(), surface.surface(), window, vk::PresentModeKHR::eMailbox};

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
}
