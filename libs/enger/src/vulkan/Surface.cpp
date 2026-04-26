#include "Surface.h"

namespace enger
{
    Surface::Surface(GlfwWindow& window, vk::Instance instance)
   {
        VkSurfaceKHR surface;
        vkCheck(vk::Result{glfwCreateWindowSurface(instance, window.nativeHandle(), nullptr, &surface)});
        m_Surface = vk::UniqueSurfaceKHR{surface, instance};
    }
}
