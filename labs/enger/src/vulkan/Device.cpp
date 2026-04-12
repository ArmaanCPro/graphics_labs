#include "Device.h"

#include <map>
#include <array>
#include <span>

#include "Descriptors.h"

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

        m_Allocator.init(instance, m_PhysicalDevice, *m_Device);

        m_GraphicsQueue = Queue(this, queueIndex, "Graphics Queue");
    }

    Device::~Device()
    {
        vkCheck(m_Device->waitIdle());
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

    Holder<ComputePipelineHandle> Device::createComputePipeline(ComputePipelineDesc desc, Queue* queue, std::string_view debugName)
    {
        auto* shaderModule = m_ShaderModulePool.get(desc.shaderModule);
        auto* layout = m_PipelineLayoutPool.get(desc.pipelineLayout);

        vk::PipelineShaderStageCreateInfo shaderStageCI{
            .stage = vk::ShaderStageFlagBits::eCompute,
            .module = *shaderModule,
            .pName = desc.entryPoint.data(),
        };

        vk::ComputePipelineCreateInfo pipelineCI{
            .stage = shaderStageCI,
            .layout = layout->layout,
        };

        vk::Pipeline pipeline = vkCheck(m_Device->createComputePipeline(nullptr, pipelineCI));

        if (!debugName.empty())
        {
            setDebugName(*m_Device, pipeline, debugName);
        }

        auto handle = m_ComputePipelinePool.create({
            .handle = pipeline,
        });

        return {this, queue, handle};
    }

    Holder<GraphicsPipelineHandle> Device::createGraphicsPipeline(GraphicsPipelineDesc desc, Queue *queue,
        std::string_view debugName)
    {
        vk::PipelineRenderingCreateInfo renderingCI{
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &desc.colorAttachment.format,
            .depthAttachmentFormat = desc.depthFormat,
            .stencilAttachmentFormat = desc.stencilFormat,
        };

        // shader stages
        auto* vertexShaderModule = m_ShaderModulePool.get(desc.vertexShaderModule);
        auto* fragmentShaderModule = m_ShaderModulePool.get(desc.fragmentShaderModule);
        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
        if (vertexShaderModule)
        {
            shaderStages.push_back(vk::PipelineShaderStageCreateInfo{
                .stage = vk::ShaderStageFlagBits::eVertex,
                .module = *vertexShaderModule,
                .pName = desc.entryPointVertex.data(),
            });
        }
        if (fragmentShaderModule)
        {
            shaderStages.push_back(vk::PipelineShaderStageCreateInfo{
                .stage = vk::ShaderStageFlagBits::eFragment,
                .module = *fragmentShaderModule,
                .pName = desc.entryPointFragment.data(),
            });
        }

        // viewport state
        vk::PipelineViewportStateCreateInfo viewportCI{
            .viewportCount = 1,
            .scissorCount = 1,
        };

        // no need for vertex input state
        vk::PipelineVertexInputStateCreateInfo vertexInputCI{};

        // Input Assembly
        vk::PipelineInputAssemblyStateCreateInfo inputAssemblyCI{
            .topology = desc.topology,
            .primitiveRestartEnable = vk::False,
        };

        // Rasterization State
        vk::PipelineRasterizationStateCreateInfo rasterCI{
            .polygonMode = desc.polygonMode,
            .cullMode = desc.cullMode,
            .frontFace = desc.frontFace,
            .lineWidth = 1.0f,
        };

        // Multisampling State
        vk::PipelineMultisampleStateCreateInfo multisampleCI{
            .rasterizationSamples = desc.sampleCount,
            .sampleShadingEnable = vk::False, // TODO parameterize multisampling
            .minSampleShading = desc.minSampleShading,
            .pSampleMask = nullptr,
            .alphaToCoverageEnable = vk::False,
            .alphaToOneEnable = vk::False,
        };

        // Depth Stencil State
        vk::PipelineDepthStencilStateCreateInfo depthStencilCI{
            .depthTestEnable = vk::False,
            .depthWriteEnable = vk::False,
            .depthCompareOp = vk::CompareOp::eNever,
            .depthBoundsTestEnable = vk::False,
            .stencilTestEnable = vk::False,
            .front = {},
            .back = {},
            .minDepthBounds = 0.0f,
            .maxDepthBounds = 1.0f,
        };

        // dummy color blending. refactor later to support transparency
        vk::PipelineColorBlendAttachmentState colorBlendAttachment{
            .blendEnable = desc.colorAttachment.blendEnabled,
            .srcColorBlendFactor = desc.colorAttachment.srcRgbBlendFactor,
            .dstColorBlendFactor = desc.colorAttachment.dstRgbBlendFactor,
            .colorBlendOp = desc.colorAttachment.rgbBlendOp,
            .srcAlphaBlendFactor = desc.colorAttachment.srcAlphaBlendFactor,
            .dstAlphaBlendFactor = desc.colorAttachment.dstAlphaBlendFactor,
            .alphaBlendOp = desc.colorAttachment.alphaBlendOp,
            .colorWriteMask = desc.colorAttachment.colorWriteMask,
        };
        vk::PipelineColorBlendStateCreateInfo colorBlendCI{
            .logicOpEnable = vk::False,
            .logicOp = vk::LogicOp::eCopy,
            .attachmentCount = 1,
            .pAttachments = &colorBlendAttachment,
        };

        std::array<vk::DynamicState, 2> dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
        vk::PipelineDynamicStateCreateInfo dynamicStateCI{
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data(),
        };
        auto* layout = m_PipelineLayoutPool.get(desc.pipelineLayout);
        vk::GraphicsPipelineCreateInfo pipelineCI{
            .pNext = &renderingCI,
            .stageCount = static_cast<uint32_t>(shaderStages.size()),
            .pStages = shaderStages.data(),
            .pVertexInputState = &vertexInputCI,
            .pInputAssemblyState = &inputAssemblyCI,
            .pTessellationState = nullptr,
            .pViewportState = &viewportCI,
            .pRasterizationState = &rasterCI,
            .pMultisampleState = &multisampleCI,
            .pDepthStencilState = &depthStencilCI,
            .pColorBlendState = &colorBlendCI,
            .pDynamicState = &dynamicStateCI,
            .layout = layout->layout,
        };

        Pipeline pipeline;
        pipeline.handle = vkCheck(m_Device->createGraphicsPipeline(nullptr, pipelineCI));
        if (!debugName.empty())
        {
            setDebugName(*m_Device, pipeline.handle, debugName);
        }

        auto handle = m_GraphicsPipelinePool.create(std::move(pipeline));
        return {this, queue, handle};
    }

    Holder<PipelineLayoutHandle> Device::createPipelineLayout(PipelineLayoutDesc desc, Queue* queue, std::string_view debugName)
    {
        std::vector<vk::DescriptorSetLayout> descriptorLayouts;
        descriptorLayouts.reserve(desc.descriptorLayouts.size());
        for (auto& dLayoutHandle : desc.descriptorLayouts)
        {
            descriptorLayouts.push_back(*m_DescriptorSetLayoutPool.get(dLayoutHandle));
        }

        // TODO think of a cleaner way to do this...
        std::vector<vk::PushConstantRange> pushConstantRanges;
        pushConstantRanges.reserve(desc.pushConstantRanges.size());
        for (auto& range : desc.pushConstantRanges)
        {
            pushConstantRanges.push_back(vk::PushConstantRange{
                .stageFlags = range.stages,
                .offset = range.offset,
                .size = range.size,
            });
        }

        vk::PipelineLayoutCreateInfo pipelineLayoutCI{
            .flags = {},
            .setLayoutCount = static_cast<uint32_t>(descriptorLayouts.size()),
            .pSetLayouts = descriptorLayouts.data(),
            .pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size()),
            .pPushConstantRanges = pushConstantRanges.data(),
        };
        vk::PipelineLayout layout = vkCheck(m_Device->createPipelineLayout(pipelineLayoutCI));

        if (!debugName.empty())
        {
            setDebugName(*m_Device, layout, debugName);
        }

        auto handle = m_PipelineLayoutPool.create({.layout = std::move(layout)});

        return {this, queue, handle};
    }

    Holder<TextureHandle> Device::createTexture(vk::Extent3D extent, vk::Format format, vk::ImageUsageFlags usage, Queue* queue, std::string_view debugName)
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

        image.allocation_ = m_Allocator.createImage(imageCI, image.image_);

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

        return {this, queue, handle};
    }

    Holder<BufferHandle> Device::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
        vk::MemoryPropertyFlags memFlags, Queue *queue, std::string_view debugName)
    {
        assert(size > 0);

        // TODO assert that sizes are within physical device limits

        VulkanBuffer buffer{
            .size_ = size,
            .usage_ = usage,
            .memoryProperties_ = memFlags
        };

        vk::BufferCreateInfo bufferCI{
            .size = size,
            .usage = usage,
            .sharingMode = vk::SharingMode::eExclusive,
        };

        // check if coherent buffer is available
        if (memFlags & vk::MemoryPropertyFlagBits::eHostVisible)
        {
            vkCheck(m_Device->createBuffer(&bufferCI, nullptr, &buffer.buffer_));
            vk::MemoryRequirements memReqs;
            m_Device->getBufferMemoryRequirements(buffer.buffer_, &memReqs);
            m_Device->destroyBuffer(buffer.buffer_, nullptr);
            buffer.buffer_ = nullptr;

            if ((memReqs.memoryTypeBits & static_cast<uint32_t>(vk::MemoryPropertyFlagBits::eHostCoherent)))
            {
                buffer.isCoherent_ = true;
            }
        }

        buffer.allocation_ = m_Allocator.createBuffer(bufferCI, buffer.buffer_, memFlags, buffer.isCoherent_, buffer.mappedMemory_);

        if (!debugName.empty())
        {
            setDebugName(*m_Device, buffer.buffer_, debugName);
        }

        // BDA
        if (usage & vk::BufferUsageFlagBits::eShaderDeviceAddress)
        {
            vk::BufferDeviceAddressInfo bufferDeviceAddressInfo{
                .buffer = buffer.buffer_,
            };
            buffer.deviceAddress_ = m_Device->getBufferAddress(bufferDeviceAddressInfo);
            assert(buffer.deviceAddress_ != 0);
        }

        return {this, queue, m_BufferPool.create(std::move(buffer))};
    }

    Holder<DescriptorSetLayoutHandle> Device::createDescriptorSetLayout(DescriptorSetLayoutDesc desc,
                                                                        Queue* queue, std::string_view debugName)
    {
        assert(desc.bindIndices.size() == desc.types.size());

        std::vector<vk::DescriptorSetLayoutBinding> bindings;
        bindings.reserve(desc.bindIndices.size());

        for (uint32_t i = 0; i < desc.bindIndices.size(); ++i)
        {
            bindings.push_back(vk::DescriptorSetLayoutBinding{
                .binding = desc.bindIndices[i],
                .descriptorType = desc.types[i],
                .descriptorCount = 1,
                .stageFlags = desc.shaderStages,
            });
        }

        vk::DescriptorSetLayoutCreateInfo layoutCI{
            .pNext = desc.pNext,
            .flags = desc.flags,
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = bindings.data(),
        };

        vk::DescriptorSetLayout set{};
        vkCheck(m_Device->createDescriptorSetLayout(&layoutCI, nullptr, &set));
        
        if (!debugName.empty())
        {
            setDebugName(*m_Device, set, debugName);
        }

        auto handle = m_DescriptorSetLayoutPool.create(std::move(set));

        return {this, queue, handle};
    }

    Holder<ShaderModuleHandle> Device::createShaderModule(std::span<const uint32_t> code, Queue* queue, std::string_view debugName)
    {
        vk::ShaderModuleCreateInfo shaderCI{
            .codeSize = code.size() * sizeof(uint32_t),
            .pCode = code.data(),
        };

        vk::ShaderModule shader = vkCheck(m_Device->createShaderModule(shaderCI));
        if (!debugName.empty())
        {
            setDebugName(*m_Device, shader, debugName);
        }

        auto handle = m_ShaderModulePool.create(std::move(shader));

        return {this, queue, handle};
    }

    void Device::destroyComputePipeline(ComputePipelineHandle handle, Queue* queue)
    {
        auto pipeline = *m_ComputePipelinePool.get(handle);
        queue = queue ? queue : &m_GraphicsQueue;

        queue->deferredDestroy([=, device = *m_Device]()
        {
            device.destroyPipeline(pipeline.handle);
        });

        m_ComputePipelinePool.destroy(handle);
    }

    void Device::destroyGraphicsPipeline(GraphicsPipelineHandle handle, Queue *queue)
    {
        auto pipeline = *m_GraphicsPipelinePool.get(handle);
        queue = queue ? queue : &m_GraphicsQueue;

        queue->deferredDestroy([=, device = *m_Device, pipeline = pipeline]()
        {
            device.destroyPipeline(pipeline.handle);
        });

        m_GraphicsPipelinePool.destroy(handle);
    }

    void Device::destroyPipelineLayout(PipelineLayoutHandle handle, Queue* queue)
    {
        auto layout = *m_PipelineLayoutPool.get(handle);
        queue = queue ? queue : &m_GraphicsQueue;

        queue->deferredDestroy([=, device = *m_Device, layout = layout]()
        {
            device.destroyPipelineLayout(layout.layout);
        });

        m_PipelineLayoutPool.destroy(handle);
    }

    void Device::destroyTexture(TextureHandle handle, Queue* queue)
    {
        auto& texture = *m_TexturePool.get(handle);
        queue = queue ? queue : &m_GraphicsQueue;

        queue->deferredDestroy([=, device = *m_Device, allocator = &m_Allocator, image = texture.image_, view = texture.view_, allocation = texture.allocation_]()
        {
            device.destroyImageView(view);
            allocator->destroyImage(allocation, image);
        });

        m_TexturePool.destroy(handle);
    }

    void Device::destroyBuffer(BufferHandle handle, Queue *queue)
    {
        auto& buffer = *m_BufferPool.get(handle);
        queue = queue ? queue : &m_GraphicsQueue;

        queue->deferredDestroy([=, device = *m_Device, allocator = &m_Allocator, buffer = buffer]()
        {
            allocator->destroyBuffer(buffer.allocation_, buffer.buffer_);
        });

        m_BufferPool.destroy(handle);
    }

    void Device::destroyDescriptorSetLayout(DescriptorSetLayoutHandle handle, Queue* queue)
    {
        auto& layout = *m_DescriptorSetLayoutPool.get(handle);
        queue = queue ? queue : &m_GraphicsQueue;

        queue->deferredDestroy([=, device = *m_Device, layout = layout]()
        {
            device.destroyDescriptorSetLayout(layout);
        });

        m_DescriptorSetLayoutPool.destroy(handle);
    }

    void Device::destroyShaderModule(ShaderModuleHandle handle, Queue* queue)
    {
        auto& shader = *m_ShaderModulePool.get(handle);
        queue = queue ? queue : &m_GraphicsQueue;

        queue->deferredDestroy([=, device = *m_Device, shader = shader]()
        {
            device.destroyShaderModule(shader);
        });

        m_ShaderModulePool.destroy(handle);
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

    TextureHandle Device::addTextureToPool(VulkanImage&& image)
    {
        return m_TexturePool.create(std::move(image));
    }

    void Device::removeTextureFromPool(TextureHandle handle)
    {
        m_TexturePool.destroy(handle);
    }
}
