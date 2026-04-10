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

        vk::SemaphoreTypeCreateInfo semaphoreTypeCI{
            .semaphoreType = vk::SemaphoreType::eTimeline,
            .initialValue = 0,
        };
        vk::SemaphoreCreateInfo semaphoreCI{
            .pNext = &semaphoreTypeCI,
        };

        m_TimelineSemaphore = vkCheck(m_Device->createSemaphoreUnique(semaphoreCI));
        setDebugName(*m_Device, *m_TimelineSemaphore, "Main Timeline Semaphore");

        m_Allocator.init(instance, m_PhysicalDevice, *m_Device);
    }

    Device::~Device()
    {
        vkCheck(m_Device->waitIdle());
        forceDeletionQueueFlush();
    }

    void Device::waitSemaphores(std::span<vk::Semaphore> semaphores, std::span<uint64_t> waitValues, uint64_t timeout)
    {
        assert(semaphores.size() == waitValues.size());

        vk::SemaphoreWaitInfo waitSemInfo{
            .semaphoreCount = static_cast<uint32_t>(semaphores.size()),
            .pSemaphores = semaphores.data(),
            .pValues = waitValues.data(),
        };
        vkCheck(m_Device->waitSemaphores(waitSemInfo, timeout));
    }

    Holder<TextureHandle> Device::createTexture(vk::Extent3D extent, vk::Format format, vk::ImageUsageFlags usage, std::string_view debugName)
    {
        VulkanImage image;
        image.extent_ = extent;
        image.format_ = format;
        image.usage_ = usage;

        vk::ImageCreateInfo imageCI{
            .imageType = vk::ImageType::e2D,
            .format = format,
            .extent = extent,
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1, // TODO parameterize (for MSAA)
            .tiling = vk::ImageTiling::eOptimal,
            .usage = usage,
            .sharingMode = vk::SharingMode::eExclusive,
            .initialLayout = vk::ImageLayout::eUndefined,
        };

        vkCheck(m_Device->createImage(&imageCI, nullptr, &image.image_));
        image.allocation_ = m_Allocator.allocateImage(imageCI, image.image_);

        vk::ImageViewCreateInfo viewCI{
            .image = image.image_,
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 },
        };

        vkCheck(m_Device->createImageView(&viewCI, nullptr, &image.view_));

        if (!debugName.empty())
        {
            setDebugName(*m_Device, image.image_, debugName);
        }

        TextureHandle handle = m_TexturePool.create(std::move(image));

        return {this, handle};
    }

    void Device::destroyComputePipeline(ComputePipelineHandle handle)
    {
        auto pipeline = *m_ComputePipelinePool.get(handle);

        m_DeletionQueue.emplace_back([=, this]()
        {
            m_Device->destroyPipeline(pipeline.handle);
        }, m_CurrentSubmitCounter);

        m_ComputePipelinePool.destroy(handle);
    }

    void Device::destroyTexture(TextureHandle handle)
    {
        auto& texture = *m_TexturePool.get(handle);

        m_DeletionQueue.emplace_back([=, this, image = texture.image_, view = texture.view_, allocation = texture.allocation_]()
        {
            m_Device->destroyImageView(view);
            m_Device->destroyImage(image);
            m_Allocator.freeImage(allocation);
        }, m_CurrentSubmitCounter);

        m_TexturePool.destroy(handle);
    }

    void Device::submitGraphics(vk::SubmitInfo2 submitInfo)
    {
        m_CurrentSubmitCounter++;
        vkCheck(m_GraphicsQueue.queue.submit2(1, &submitInfo, nullptr));
    }

    UniqueCommandPool Device::createUniqueCommandPool(CommandPoolFlags flags, uint32_t queueFamilyIndex, std::string_view debugName)
    {
        vk::CommandPoolCreateInfo commandPoolCI{
            .flags = flags == CommandPoolFlags::ResetCommandBuffer
                ? vk::CommandPoolCreateFlagBits::eResetCommandBuffer : vk::CommandPoolCreateFlagBits::eTransient,
            .queueFamilyIndex = queueFamilyIndex,
        };

        auto commandPool = vkCheck(m_Device->createCommandPoolUnique(commandPoolCI));

        if (!debugName.empty())
        {
            setDebugName(*m_Device, *commandPool, debugName.data());
        }

        return UniqueCommandPool{std::move(commandPool),
            queueFamilyIndex};
    }

    std::vector<UniqueCommandPool> Device::createUniqueCommandPools(CommandPoolFlags flags, uint32_t queueFamilyIndex,
        uint32_t count, std::string_view debugName)
    {
        vk::CommandPoolCreateInfo commandPoolCI{
            .flags = flags == CommandPoolFlags::ResetCommandBuffer
                ? vk::CommandPoolCreateFlagBits::eResetCommandBuffer : vk::CommandPoolCreateFlagBits::eTransient,
            .queueFamilyIndex = queueFamilyIndex,
        };

        std::vector<UniqueCommandPool> result;
        result.reserve(count);
        for (uint32_t i = 0; i < count; ++i)
        {
            auto commandPool = vkCheck(m_Device->createCommandPoolUnique(commandPoolCI));
            if (!debugName.empty())
            {
                setDebugName(*m_Device, *commandPool, debugName.data() + std::to_string(i));
            }
            result.push_back(UniqueCommandPool{std::move(commandPool),
                queueFamilyIndex});
        }

        return result;
    }

    CommandBuffer Device::allocateCommandBuffer(UniqueCommandPool &commandPool, CommandBufferLevel level, std::string_view debugName)
    {
        vk::CommandBufferAllocateInfo cmdAllocCI{
            .commandPool = *commandPool.m_CommandPool,
            .level = level == CommandBufferLevel::Primary ? vk::CommandBufferLevel::ePrimary : vk::CommandBufferLevel::eSecondary,
            .commandBufferCount = 1,
        };
        auto cmdBuffer = vkCheck(m_Device->allocateCommandBuffers(cmdAllocCI)).front();

        if (!debugName.empty())
        {
            setDebugName(*m_Device, cmdBuffer, debugName.data());
        }

        return {this, cmdBuffer};
    }

    std::vector<CommandBuffer> Device::allocateCommandBuffers(UniqueCommandPool &commandPool, CommandBufferLevel level,
                                                              uint32_t count, std::string_view debugName)
    {
        vk::CommandBufferAllocateInfo cmdAllocCI{
            .commandPool = *commandPool.m_CommandPool,
            .level = level == CommandBufferLevel::Primary ? vk::CommandBufferLevel::ePrimary : vk::CommandBufferLevel::eSecondary,
            .commandBufferCount = count,
        };
        auto cmdBuffers = vkCheck(m_Device->allocateCommandBuffers(cmdAllocCI));
        std::vector<CommandBuffer> result;
        result.reserve(count);
        for (uint32_t i = 0; i < count; ++i)
        {
            if (!debugName.empty())
            {
                setDebugName(*m_Device, cmdBuffers[i], debugName.data() + std::to_string(i));
            }
            result.push_back({this, cmdBuffers[i]});
        }
        return result;
    }

    void Device::flushDeletionQueue()
    {
        uint64_t gpuSubmitValue = vkCheck(m_Device->getSemaphoreCounterValue(*m_TimelineSemaphore));

        std::erase_if(m_DeletionQueue, [&](const auto &task)
        {
            if (task.submitValue <= gpuSubmitValue)
            {
                task.func();
                return true;
            }
            return false;
        });
    }

    void Device::forceDeletionQueueFlush()
    {
        for (auto &task : m_DeletionQueue)
        {
            task.func();
        }
        m_DeletionQueue.clear();
    }

    TextureHandle Device::addTextureToPool(VulkanImage&& image)
    {
        return m_TexturePool.create(std::move(image));
    }

    void Device::removeTextureFromPool(TextureHandle handle)
    {
        m_TexturePool.destroy(handle);
    }
}
