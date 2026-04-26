#include "Surface.h"

#include <GLFW/glfw3.h>

namespace enger
{
    Surface::Surface(Window& window, vk::Instance instance)
   {
        VkSurfaceKHR surface;
        vkCheck(vk::Result{glfwCreateWindowSurface(instance, static_cast<GLFWwindow*>(window.nativeHandle()), nullptr, &surface)});
        m_Surface = vk::UniqueSurfaceKHR{surface, instance};
    }
}
