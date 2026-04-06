#include <array>
#include <span>

#include "vulkan/Device.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

constexpr auto WIDTH = 800;
constexpr auto HEIGHT = 600;

int main()
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Enger", nullptr, nullptr);

    std::vector<const char*> extensions;
#ifndef NDEBUG
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    uint32_t glfwExtensionCount = 0;
    auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    extensions.insert(extensions.end(), glfwExtensions, glfwExtensions + glfwExtensionCount);

    // maybe: don't make it vulkan specific, instead maybe map some generic extension names to vulkan specific
    std::array<const char*, 1> requiredDeviceExtensions = {
        vk::KHRSwapchainExtensionName
    };

    auto device = enger::Device{extensions, requiredDeviceExtensions};

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
}
