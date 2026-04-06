#include "Device.h"

#include <map>
#include <array>
#include <span>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace enger
{
    // make sure our instance has all the required extensions (most likely by the WSI/GLFW)
    void checkRequiredInstanceExtensions(const std::vector<vk::ExtensionProperties>& availableInstExt, std::span<const char*> reqInstExtensions)
    {
        for (auto reqInstExtension : reqInstExtensions)
        {
            if (std::ranges::none_of(availableInstExt,
                [instanceExtension = reqInstExtension](const auto& extension)
                { return std::strcmp(extension.extensionName, instanceExtension) == 0; }))
            {
                std::cerr << "Missing instance extension: " << reqInstExtension << std::endl;
                std::terminate();
            }
        }
    }

    // returns physical devices sorted from worst to best
    std::multimap<int, vk::PhysicalDevice> sortPhysicalDevices(const std::vector<vk::PhysicalDevice>& physicalDevices, std::span<const char*> requiredDeviceExtensions)
    {
        std::multimap<int, vk::PhysicalDevice> sortedDevices;
        for (auto& device : physicalDevices)
        {
            auto deviceProps = device.getProperties();
            auto deviceFeatures = device.getFeatures();

            uint32_t score = 0;
            if (deviceProps.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
            {
                score += 100;
            }

            bool supportsVulkan14 = deviceProps.apiVersion >= VK_API_VERSION_1_4;

            auto queueFamilies = device.getQueueFamilyProperties();
            bool supportsDesiredQueues = std::ranges::any_of(queueFamilies,
                [](const auto& qfp)
                {
                    return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
                });

            auto availableDeviceExtensions = vkCheck(device.enumerateDeviceExtensionProperties());
            bool supportsAllRequiredExtensions = std::ranges::all_of(requiredDeviceExtensions,
                [&availableDeviceExtensions](const auto& requiredDeviceExtension)
                {
                    return std::ranges::any_of(availableDeviceExtensions,
                        [requiredDeviceExtension](const auto& availableDeviceExtension)
                        {
                            return std::strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0;
                        });
                });

            auto features = device.getFeatures2<
                vk::PhysicalDeviceFeatures2,
                vk::PhysicalDeviceVulkan13Features,
                vk::PhysicalDeviceVulkan14Features,
                vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

            bool supportsRequiredFeatures = features.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering
                && features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

            if (!supportsVulkan14 || !supportsDesiredQueues || !supportsAllRequiredExtensions || !supportsRequiredFeatures)
            {
                continue;
            }

            sortedDevices.emplace(score, device);
        }
        return sortedDevices;
    }

    Device::Device(std::span<const char*> instanceExtensions, std::span<const char*> deviceExtensions)
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


        // physical device selection
        const std::vector<vk::PhysicalDevice> physicalDevices = vkCheck(m_Instance->enumeratePhysicalDevices());
        auto sortedDevices = sortPhysicalDevices(physicalDevices, deviceExtensions);
        if (!sortedDevices.empty() && sortedDevices.rbegin()->first == 0)
        {
            std::cerr << "No suitable device found!" << std::endl;
            std::terminate();
        }
        m_PhysicalDevice = sortedDevices.rbegin()->second;

        // logical device creation
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = m_PhysicalDevice.getQueueFamilyProperties();
        auto graphicsQueueFamily = std::ranges::find_if(queueFamilyProperties,
            [](const auto& qfp)
            {
                return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
            });
        auto graphicsIndex = static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamily));

        float queuePriority = 1.0f;
        vk::DeviceQueueCreateInfo queueCreateInfo{
            .queueFamilyIndex = graphicsIndex,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        };

        vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceVulkan14Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain {
            {},
            {.dynamicRendering = true},
            {},
            {.extendedDynamicState = true}
        };

        vk::DeviceCreateInfo deviceCreateInfo{
            .sType = vk::StructureType::eCommandBufferAllocateInfo,
            .pNext = &featureChain.get(),
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queueCreateInfo,
            .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
            .ppEnabledExtensionNames = deviceExtensions.data()
        };

        m_Device = vkCheck(m_PhysicalDevice.createDeviceUnique(deviceCreateInfo));
        VULKAN_HPP_DEFAULT_DISPATCHER.init(*m_Device);

        // load debug utils (it is an instance extension, not device extension, so it doesn't get picked up by the dispatcher)
        VULKAN_HPP_DEFAULT_DISPATCHER.vkSetDebugUtilsObjectNameEXT =
            reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
                m_Instance->getProcAddr("vkSetDebugUtilsObjectNameEXT")
            );
        setDebugName(*m_Device, *m_Device, "Main Logical Device");

    }
}
