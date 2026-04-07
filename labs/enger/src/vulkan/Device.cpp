#include "Device.h"

#include <map>
#include <array>
#include <span>

namespace enger
{
    // returns physical devices sorted from worst to best
    std::multimap<int, vk::PhysicalDevice> sortPhysicalDevices(const std::vector<vk::PhysicalDevice> &physicalDevices,
                                                               std::span<const char *> requiredDeviceExtensions)
    {
        std::multimap<int, vk::PhysicalDevice> sortedDevices;
        for (auto &device: physicalDevices)
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
                                                             [](const auto &qfp)
                                                             {
                                                                 return !!(qfp.queueFlags &
                                                                           vk::QueueFlagBits::eGraphics);
                                                             });

            auto availableDeviceExtensions = vkCheck(device.enumerateDeviceExtensionProperties());
            bool supportsAllRequiredExtensions = std::ranges::all_of(requiredDeviceExtensions,
                                                                     [&availableDeviceExtensions](
                                                                     const auto &requiredDeviceExtension)
                                                                     {
                                                                         return std::ranges::any_of(
                                                                             availableDeviceExtensions,
                                                                             [requiredDeviceExtension](
                                                                             const auto &availableDeviceExtension)
                                                                             {
                                                                                 return std::strcmp(
                                                                                     availableDeviceExtension.
                                                                                     extensionName,
                                                                                     requiredDeviceExtension) == 0;
                                                                             });
                                                                     });

            auto features = device.getFeatures2<
                vk::PhysicalDeviceFeatures2,
                vk::PhysicalDeviceVulkan12Features,
                vk::PhysicalDeviceVulkan13Features,
                vk::PhysicalDeviceVulkan14Features,
                vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();

            bool supportsRequiredFeatures = features.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering
                                            && features.get<vk::PhysicalDeviceVulkan13Features>().synchronization2
                                            && features.get<vk::PhysicalDeviceVulkan12Features>().bufferDeviceAddress
                                            && features.get<vk::PhysicalDeviceVulkan12Features>().timelineSemaphore
                                            && features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().
                                            extendedDynamicState;

            if (!supportsVulkan14 || !supportsDesiredQueues || !supportsAllRequiredExtensions || !
                supportsRequiredFeatures)
            {
                continue;
            }

            sortedDevices.emplace(score, device);
        }
        return sortedDevices;
    }

    Device::Device(vk::Instance instance, vk::SurfaceKHR surface, std::span<const char *> deviceExtensions)
    {
        // physical device selection
        const std::vector<vk::PhysicalDevice> physicalDevices = vkCheck(instance.enumeratePhysicalDevices());
        auto sortedDevices = sortPhysicalDevices(physicalDevices, deviceExtensions);
        if (!sortedDevices.empty() && sortedDevices.rbegin()->first == 0)
        {
            std::cerr << "No suitable GPU/device found!" << std::endl;
            std::terminate();
        }
        m_PhysicalDevice = sortedDevices.rbegin()->second;

        // logical device creation
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = m_PhysicalDevice.getQueueFamilyProperties();
        uint32_t queueIndex = ~0u;
        for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); ++qfpIndex)
        {
            auto &qfp = queueFamilyProperties[qfpIndex];
            if ((qfp.queueFlags & vk::QueueFlagBits::eGraphics)
                && vkCheck(m_PhysicalDevice.getSurfaceSupportKHR(qfpIndex, surface)))
            {
                queueIndex = qfpIndex;
                break;
            }
        }
        // TODO replace this assert with something cleaner. In fact, the current cerr + terminate should be cleaner as well.
        assert(queueIndex != ~0 && "No graphics queue family found");

        float queuePriority = 1.0f;
        vk::DeviceQueueCreateInfo queueCreateInfo{
            .queueFamilyIndex = queueIndex,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        };

        vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan12Features,
                           vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceVulkan14Features,
                           vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain{
            {},
            {.timelineSemaphore = true, .bufferDeviceAddress = true },
            {.synchronization2 = true, .dynamicRendering = true},
            {},
            {.extendedDynamicState = true}
        };

        vk::DeviceCreateInfo deviceCreateInfo{
            .pNext = &featureChain.get(),
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &queueCreateInfo,
            .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
            .ppEnabledExtensionNames = deviceExtensions.data()
        };

        m_Device = vkCheck(m_PhysicalDevice.createDeviceUnique(deviceCreateInfo));
        VULKAN_HPP_DEFAULT_DISPATCHER.init(*m_Device);

        setDebugName(*m_Device, *m_Device, "Main Logical Device");

        m_GraphicsQueue.queue = m_Device->getQueue(queueIndex, 0);
        setDebugName(*m_Device, m_GraphicsQueue.queue, "Graphics Queue");
        m_GraphicsQueue.index = queueIndex;
    }

    void Device::destroyComputePipeline(ComputePipelineHandle handle)
    {
        auto pipeline = *m_ComputePipelinePool.get(handle);
        m_Device->destroyPipeline(pipeline.handle);

        m_ComputePipelinePool.destroy(handle);
    }
}
