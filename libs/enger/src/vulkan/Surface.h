#pragma once

#include "vk.h"

#include "GlfwWindow.h"

namespace enger
{
    class Surface
    {
    public:
        Surface(GlfwWindow& window, vk::Instance instance)
        {
            VkSurfaceKHR surface;
            vkCheck(vk::Result{glfwCreateWindowSurface(instance, window.nativeHandle(), nullptr, &surface)});
            m_Surface = vk::UniqueSurfaceKHR{surface, instance};
        }

        vk::SurfaceKHR surface() { return *m_Surface; }

    private:
        /// In this case, the unique handle stores a handle to the instance.
        /// Fine for long-lasting items like the Surface, but just be wary of Unique handles for shorter lived items.
        vk::UniqueSurfaceKHR m_Surface;
    };
}
