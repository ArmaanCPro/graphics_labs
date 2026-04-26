#include "Instance.h"

namespace enger
{
    // make sure our instance has all the required extensions (most likely by the WSI/GLFW)
    void checkRequiredInstanceExtensions(const std::vector<vk::ExtensionProperties> &availableInstExt,
                                         std::span<const char *> reqInstExtensions)
    {
        for (auto reqInstExtension: reqInstExtensions)
        {
            if (std::ranges::none_of(availableInstExt,
                                     [instanceExtension = reqInstExtension](const auto &extension)
                                     {
                                         return std::strcmp(extension.extensionName, instanceExtension) == 0;
                                     }))
            {
                LOG_ERROR("Missing instance extension: {}", reqInstExtension);
                std::terminate();
            }
        }
    }

    Instance::Instance(std::span<const char *> instanceExtensions)
    {
        auto vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);

        constexpr vk::ApplicationInfo appInfo{
            .pApplicationName = "Enger",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = "No Engine",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = vk::ApiVersion14
        };
        vk::InstanceCreateInfo info{
            .pApplicationInfo = &appInfo,
            .enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size()),
            .ppEnabledExtensionNames = instanceExtensions.data(),
        };

        m_Instance = vkCheck(vk::createInstanceUnique(info));

        VULKAN_HPP_DEFAULT_DISPATCHER.init(*m_Instance);

        auto extensionProps = vkCheck(vk::enumerateInstanceExtensionProperties());
        checkRequiredInstanceExtensions(extensionProps, instanceExtensions);

        // we could enable validation layers here...
        // or just use Vulkan Configurator (vkconfig)

#ifndef NDEBUG
        // load debug utils (it is an instance extension, not device extension, so it doesn't get picked up by the dispatcher)
        VULKAN_HPP_DEFAULT_DISPATCHER.vkSetDebugUtilsObjectNameEXT =
            reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
                m_Instance->getProcAddr("vkSetDebugUtilsObjectNameEXT")
            );
#endif
    }
}
