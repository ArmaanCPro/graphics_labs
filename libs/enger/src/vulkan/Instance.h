#pragma once

#include <span>

#include "vk.h"

namespace enger
{
    /// Instance should be the top-level Vulkan object.
    /// It should only be destroyed AFTER all other Vulkan objects. RAII enforces this, but be careful.
    class Instance
    {
    public:
        Instance(std::span<const char*> instanceExtensions);

        vk::Instance instance() { return *m_Instance; }

    private:
        /// Must be the first member, as dl owns the instance (vulkan-1.dll) and must outlive all Vulkan handles.
        /// Also, the instance functions are rarely used, as once the dispatcher is initialized with the device, it goes straight to the driver.
        vk::detail::DynamicLoader dl;

        vk::UniqueInstance m_Instance;
    };
}
