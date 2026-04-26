#pragma once

#include "vk.h"

#include "Window.h"

namespace enger
{
    class ENGER_EXPORT Surface final
    {
    public:
        Surface(Window& window, vk::Instance instance);

        vk::SurfaceKHR surface() { return *m_Surface; }

    private:
        /// In this case, the unique handle stores a handle to the instance.
        /// Fine for long-lasting items like the Surface, but just be wary of Unique handles for shorter lived items.
        vk::UniqueSurfaceKHR m_Surface;
    };
}
