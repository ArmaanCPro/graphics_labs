#include "Device.h"

#include <map>
#include <array>
#include <span>

#include "Descriptors.h"
#include "PushConstantRanges.h"

namespace enger
{
    std::multimap<int, std::pair<PhysicalDeviceInfo, const vk::PhysicalDevice> > Device::sortPhysicalDevices(
        const std::vector<vk::PhysicalDevice>& physicalDevices,
        std::span<const char*> requiredDeviceExtensions)
    {
        std::multimap<int, std::pair<PhysicalDeviceInfo, const vk::PhysicalDevice> > sortedDevices;
        for (auto& device: physicalDevices)
        {
            PhysicalDeviceInfo infos;
            infos.properties = device.getProperties2();
            infos.features = device.getFeatures2();

            uint32_t score = 0;
            if (infos.properties.properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
            {
                score += 100;
            }

            bool supportsVulkan14 = infos.properties.properties.apiVersion >= VK_API_VERSION_1_4;

            infos.queueFamilyProperties = device.getQueueFamilyProperties();
            bool supportsDesiredQueues = std::ranges::any_of(infos.queueFamilyProperties,
                                                             [](const auto& qfp) {
                                                                 return !!(qfp.queueFlags &
                                                                           vk::QueueFlagBits::eGraphics);
                                                             });

            infos.availableExtensions = vkCheck(device.enumerateDeviceExtensionProperties());
            const auto& availableDeviceExtensions = infos.availableExtensions;
            bool supportsAllRequiredExtensions = std::ranges::all_of(requiredDeviceExtensions,
                                                                     [&availableDeviceExtensions](
                                                                     const auto& requiredDeviceExtension) {
                                                                         return std::ranges::any_of(
                                                                             availableDeviceExtensions,
                                                                             [requiredDeviceExtension](
                                                                             const auto& availableDeviceExtension) {
                                                                                 return std::strcmp(
                                                                                     availableDeviceExtension.
                                                                                     extensionName,
                                                                                     requiredDeviceExtension) == 0;
                                                                             });
                                                                     });

            infos.availableLayers = vkCheck(device.enumerateDeviceLayerProperties());
            for (const auto& availableExtension: availableDeviceExtensions)
            {
                if (std::strcmp(availableExtension.extensionName, vk::KHRCalibratedTimestampsExtensionName) == 0)
                {
                    infos.hasKhrCalibratedTimestamps = true;
                    score += 10;
                }
                if (std::strcmp(availableExtension.extensionName, vk::EXTCalibratedTimestampsExtensionName) == 0)
                {
                    infos.hasExtCalibratedTimestamps = true;
                    score += 10;
                }
                if (std::strcmp(availableExtension.extensionName, vk::KHRMaintenance9ExtensionName) == 0)
                {
                    infos.hasMaintenance9 = true;
                    score += 10;
                }
                if (std::strcmp(availableExtension.extensionName, vk::EXTRobustness2ExtensionName) == 0)
                {
                    infos.hasRobustness2 = true;
                    score += 10;
                }
            }

            auto features = device.getFeatures2<
                vk::PhysicalDeviceFeatures2,
                vk::PhysicalDeviceVulkan12Features,
                vk::PhysicalDeviceVulkan13Features,
                vk::PhysicalDeviceVulkan14Features,
                vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT,
                vk::PhysicalDeviceRobustness2FeaturesEXT>(); // currently only supporting EXT version. Consider adding KHR

            bool supportsRequiredFeatures = features.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering
                                            && features.get<vk::PhysicalDeviceVulkan13Features>().synchronization2
                                            && features.get<vk::PhysicalDeviceVulkan12Features>().bufferDeviceAddress
                                            && features.get<vk::PhysicalDeviceVulkan12Features>().timelineSemaphore
                                            && features.get<vk::PhysicalDeviceVulkan12Features>().descriptorIndexing
                                            && features.get<vk::PhysicalDeviceVulkan12Features>().
                                            shaderStorageImageArrayNonUniformIndexing
                                            && features.get<vk::PhysicalDeviceVulkan12Features>().
                                            descriptorBindingStorageImageUpdateAfterBind
                                            && features.get<vk::PhysicalDeviceVulkan12Features>().
                                            descriptorBindingSampledImageUpdateAfterBind
                                            && features.get<vk::PhysicalDeviceVulkan12Features>().runtimeDescriptorArray
                                            && features.get<vk::PhysicalDeviceVulkan12Features>().
                                            descriptorBindingPartiallyBound
                                            && features.get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().
                                            extendedDynamicState
                                            && features.get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy
                                            && features.get<vk::PhysicalDeviceFeatures2>().features.textureCompressionBC;

            infos.hasHostQuery = features.get<vk::PhysicalDeviceVulkan12Features>().hostQueryReset;
            if (infos.hasHostQuery)
            {
                score += 10;
            }
            infos.hasNullDescriptor = features.get<vk::PhysicalDeviceRobustness2FeaturesEXT>().nullDescriptor;
            infos.hasPipelineRobustness = features.get<vk::PhysicalDeviceVulkan14Features>().pipelineRobustness;

            if (!supportsVulkan14 || !supportsDesiredQueues || !supportsAllRequiredExtensions || !
                supportsRequiredFeatures)
            {
                continue;
            }

            sortedDevices.emplace(
                score,
                std::make_pair<PhysicalDeviceInfo, const vk::PhysicalDevice>(std::move(infos), std::move(device)));
        }
        return sortedDevices;
    }

    Device::Device(vk::Instance instance, vk::SurfaceKHR surface, std::vector<const char*> deviceExtensions,
                   bool useBindless)
        :
        m_UseBindless(useBindless)
    {
        ENGER_PROFILE_FUNCTION()
        // Swapchain Mutable Format is nearly universal on Vulkan 1.4 desktop (mobile support is a little bit more shoddy)
        deviceExtensions.push_back(vk::KHRSwapchainMutableFormatExtensionName);

        // physical device selection
        const std::vector<vk::PhysicalDevice> physicalDevices = vkCheck(instance.enumeratePhysicalDevices());
        const auto sortedDevices = sortPhysicalDevices(physicalDevices, deviceExtensions);
        if (!sortedDevices.empty() && sortedDevices.rbegin()->first == 0)
        {
            LOG_ERROR("No suitable GPU/device found! Consider updating drivers.");
            std::terminate();
        }
        const auto [deviceInfo, physicalDevice] = sortedDevices.rbegin()->second;
        m_PhysicalDevice = physicalDevice;
        m_DeviceInfo = deviceInfo;

        if (m_UseBindless)
        {
            // nullDescriptor isn't truly required, but highly beneficial for bindless rendering.
            assert(m_DeviceInfo.hasRobustness2 && m_DeviceInfo.hasNullDescriptor && "For bindless rendering, NullDescriptor (from Robustness2) is required");
        }

        // logical device creation
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = m_PhysicalDevice.getQueueFamilyProperties();
        uint32_t graphicsQueueIndex = ~0u;
        uint32_t transferQueueIndex = ~0u;
        for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); ++qfpIndex)
        {
            auto& qfp = queueFamilyProperties[qfpIndex];
            // Graphics Queue selection
            if ((qfp.queueFlags & vk::QueueFlagBits::eGraphics)
                && vkCheck(m_PhysicalDevice.getSurfaceSupportKHR(qfpIndex, surface)))
            {
                graphicsQueueIndex = graphicsQueueIndex == ~0 ? qfpIndex : graphicsQueueIndex;
            }

            // Transfer Queue selection
            if (qfp.queueFlags & vk::QueueFlagBits::eTransfer)
            {
                // Priority 1: Dedicated Transfer Queue
                if (!(qfp.queueFlags & (vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute)))
                {
                    transferQueueIndex = qfpIndex;
                }
                // Priority 2: Any transfer-capable queue
                else if (transferQueueIndex == ~0)
                {
                    transferQueueIndex = qfpIndex;
                }
            }
        }
        assert(graphicsQueueIndex != ~0 && "No graphics queue family found");

        float graphicsQueuePriority = 1.0f;
        vk::DeviceQueueCreateInfo graphicsQueueCI{
            .queueFamilyIndex = graphicsQueueIndex,
            .queueCount = 1,
            .pQueuePriorities = &graphicsQueuePriority
        };

        float transferQueuePriority = 1.0f;
        vk::DeviceQueueCreateInfo transferQueueCI{
            .queueFamilyIndex = transferQueueIndex,
            .queueCount = 1,
            .pQueuePriorities = &transferQueuePriority
        };

        vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan12Features,
            vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceVulkan14Features,
            vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT, vk::PhysicalDeviceRobustness2FeaturesEXT>
            featureChain{
            {
                .features = vk::PhysicalDeviceFeatures{
                    .samplerAnisotropy = true,
                    .textureCompressionBC = true, // TODO this is nearly guaranteed (on desktop), but still useful to make sure
                },
            },
            {
                .descriptorIndexing = true, .shaderStorageImageArrayNonUniformIndexing = true,
                .descriptorBindingSampledImageUpdateAfterBind = true,
                .descriptorBindingStorageImageUpdateAfterBind = true,
                .descriptorBindingPartiallyBound = true, .runtimeDescriptorArray = true,
#ifdef ENABLE_PROFILING
                // If using HostCalibrated, Tracy needs both Host Query & Calibrated Timestamps
                .hostQueryReset = m_DeviceInfo.hasHostQuery && (
                                      m_DeviceInfo.hasKhrCalibratedTimestamps || m_DeviceInfo.
                                      hasExtCalibratedTimestamps),
#endif
                .timelineSemaphore = true, .bufferDeviceAddress = true,
            },
            {.synchronization2 = true, .dynamicRendering = true},
            {.pipelineRobustness = m_DeviceInfo.hasPipelineRobustness},
            {.extendedDynamicState = true},
            {.nullDescriptor = m_UseBindless && m_DeviceInfo.hasNullDescriptor}
        };

#ifdef ENABLE_PROFILING
        if (m_DeviceInfo.hasKhrCalibratedTimestamps)
        {
            assert(m_DeviceInfo.hasExtCalibratedTimestamps && "Right now, Tracy needs the EXT version as well. (for me) Consider fixing");
            deviceExtensions.push_back(vk::KHRCalibratedTimestampsExtensionName);
        }
        if (m_DeviceInfo.hasExtCalibratedTimestamps)
        {
            deviceExtensions.push_back(vk::EXTCalibratedTimestampsExtensionName);
        }
#endif

        if (m_DeviceInfo.hasMaintenance9)
        {
            deviceExtensions.push_back(vk::KHRMaintenance9ExtensionName);
        }

        std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
        queueCreateInfos.push_back(graphicsQueueCI);
        if (transferQueueIndex != ~0)
        {
            queueCreateInfos.push_back(transferQueueCI);
        }

        vk::DeviceCreateInfo deviceCreateInfo{
            .pNext = &featureChain.get(),
            .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
            .pQueueCreateInfos = queueCreateInfos.data(),
            .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
            .ppEnabledExtensionNames = deviceExtensions.data()
        };

        m_Device = vkCheck(m_PhysicalDevice.createDeviceUnique(deviceCreateInfo));
        VULKAN_HPP_DEFAULT_DISPATCHER.init(*m_Device);

        setDebugName(*m_Device, *m_Device, "Main Logical Device");

        m_Allocator.init(instance, m_PhysicalDevice, *m_Device);

        m_GraphicsQueue = Queue(this, graphicsQueueIndex, "Graphics Queue");

        if (transferQueueIndex != ~0)
        {
            m_TransferQueue = std::make_optional<Queue>(Queue{
                this, transferQueueIndex,
                "Transfer Queue"
            });
        }

        if (m_UseBindless)
        {
            initBindless();
        }

#ifdef ENABLE_PROFILING
        std::vector<vk::TimeDomainKHR> timeDomainsKhr;
        std::vector<vk::TimeDomainEXT> timeDomainsExt;
        if (m_DeviceInfo.hasKhrCalibratedTimestamps)
        {
            timeDomainsKhr = vkCheck(m_PhysicalDevice.getCalibrateableTimeDomainsKHR());
        }
        else if (m_DeviceInfo.hasExtCalibratedTimestamps)
        {
            timeDomainsExt = vkCheck(m_PhysicalDevice.getCalibrateableTimeDomainsEXT());
        }
        const bool hasHostQuery = m_DeviceInfo.hasHostQuery && [&]() -> bool {
            if (m_DeviceInfo.hasKhrCalibratedTimestamps)
            {
                for (vk::TimeDomainKHR domain: timeDomainsKhr)
                {
                    if (domain == vk::TimeDomainKHR::eClockMonotonicRaw ||
                        domain == vk::TimeDomainKHR::eQueryPerformanceCounter)
                    {
                        return true;
                    }
                }
            }
            else if (m_DeviceInfo.hasExtCalibratedTimestamps)
            {
                for (vk::TimeDomainEXT domain: timeDomainsExt)
                {
                    if (domain == vk::TimeDomainEXT::eClockMonotonicRaw ||
                        domain == vk::TimeDomainEXT::eQueryPerformanceCounter)
                    {
                        return true;
                    }
                }
            }
            return false;
        }();
        if (hasHostQuery)
        {
            // If we have Host Query Reset, we don't need to allocate a separate command buffer.
            m_UsingTracyHostCalibrated = true;
            m_TracyVkCtx = TracyVkContextHostCalibrated(instance, m_PhysicalDevice, *m_Device,
                                                        VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr,
                                                        VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr);
        }
        else // allocate the command buffers
        {
            const vk::CommandPoolCreateInfo commandPoolCI{
                .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient,
                .queueFamilyIndex = m_GraphicsQueue.familyIndex(),
            };
            m_TracyCommandPool = vkCheck(m_Device->createCommandPoolUnique(commandPoolCI));
            setDebugName(*m_Device, *m_TracyCommandPool, "Tracy Command Pool");
            const vk::CommandBufferAllocateInfo commandBufferCI{
                .commandPool = *m_TracyCommandPool,
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = 1,
            };
            m_TracyCommandBuffer = vkCheck(m_Device->allocateCommandBuffers(commandBufferCI)).front();
        }

        if (!m_UsingTracyHostCalibrated && (m_DeviceInfo.hasKhrCalibratedTimestamps || m_DeviceInfo.
                                            hasExtCalibratedTimestamps))
        {
            m_TracyVkCtx = TracyVkContextCalibrated(instance, m_PhysicalDevice, *m_Device, m_GraphicsQueue.queue(),
                                                    m_TracyCommandBuffer,
                                                    VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr,
                                                    VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr);
        }
        else if (!m_UsingTracyHostCalibrated)
        {
            m_TracyVkCtx = TracyVkContext(instance, m_PhysicalDevice, *m_Device, m_GraphicsQueue.queue(),
                                          m_TracyCommandBuffer,
                                          VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr,
                                          VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr);
        }
#endif

        m_DefaultNullSampler = createSampler(SamplerDesc{
        }, &m_GraphicsQueue, "Default Null Sampler");
    }

    Device::~Device()
    {
        ENGER_PROFILE_FUNCTION()
        vkCheck(m_Device->waitIdle());
#ifdef ENABLE_PROFILING
        TracyVkDestroy(m_TracyVkCtx);
#endif
    }

    void Device::waitSemaphores(std::span<const vk::Semaphore> semaphores, std::span<const uint64_t> waitValues,
                                uint64_t timeout)
    {
        ENGER_PROFILE_FUNCTION()
        assert(semaphores.size() == waitValues.size());

        vk::SemaphoreWaitInfo waitSemInfo{
            .semaphoreCount = static_cast<uint32_t>(semaphores.size()),
            .pSemaphores = semaphores.data(),
            .pValues = waitValues.data(),
        };
        vkCheck(m_Device->waitSemaphores(waitSemInfo, timeout));
    }

    Holder<ComputePipelineHandle> Device::createComputePipeline(ComputePipelineDesc desc, Queue* queue,
                                                                std::string_view debugName)
    {
        // Robustness
        vk::PipelineRobustnessCreateInfo robustnessCI{
            .storageBuffers = vk::PipelineRobustnessBufferBehavior::eDisabled,
            .uniformBuffers = vk::PipelineRobustnessBufferBehavior::eDisabled,
            .vertexInputs = vk::PipelineRobustnessBufferBehavior::eDisabled,
            .images = vk::PipelineRobustnessImageBehavior::eDisabled,
        };
        const bool usingRobustness = desc.enablePipelineRobustness && m_DeviceInfo.hasPipelineRobustness;
        if (desc.enablePipelineRobustness && m_DeviceInfo.hasPipelineRobustness)
        {
#ifdef NDEBUG
            LOG_ERROR("Consider disabling pipeline robustness on Release builds [{}]", debugName);
#endif
            robustnessCI.storageBuffers = vk::PipelineRobustnessBufferBehavior::eRobustBufferAccess2;
            robustnessCI.uniformBuffers = vk::PipelineRobustnessBufferBehavior::eRobustBufferAccess2;
            robustnessCI.images = vk::PipelineRobustnessImageBehavior::eRobustImageAccess2;
            robustnessCI.vertexInputs = vk::PipelineRobustnessBufferBehavior::eRobustBufferAccess2;
        }

        auto* shaderModule = m_ShaderModulePool.get(desc.shaderModule);
        auto* layout = m_PipelineLayoutPool.get(desc.pipelineLayout);

        vk::PipelineShaderStageCreateInfo shaderStageCI{
            .stage = vk::ShaderStageFlagBits::eCompute,
            .module = *shaderModule,
            .pName = desc.entryPoint.data(),
        };

        vk::ComputePipelineCreateInfo pipelineCI{
            .pNext = usingRobustness ? &robustnessCI : nullptr,
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

    Holder<GraphicsPipelineHandle> Device::createGraphicsPipeline(GraphicsPipelineDesc desc, Queue* queue,
                                                                  std::string_view debugName)
    {
        // Robustness
        vk::PipelineRobustnessCreateInfo robustnessCI{
            .storageBuffers = vk::PipelineRobustnessBufferBehavior::eDisabled,
            .uniformBuffers = vk::PipelineRobustnessBufferBehavior::eDisabled,
            .vertexInputs = vk::PipelineRobustnessBufferBehavior::eDisabled,
            .images = vk::PipelineRobustnessImageBehavior::eDisabled,
        };
        const bool usingRobustness = desc.enablePipelineRobustness && m_DeviceInfo.hasPipelineRobustness;
        if (usingRobustness)
        {
#ifdef NDEBUG
            LOG_ERROR("Consider disabling pipeline robustness on Release builds [{}]", debugName);
#endif
            robustnessCI.storageBuffers = vk::PipelineRobustnessBufferBehavior::eRobustBufferAccess2;
            robustnessCI.uniformBuffers = vk::PipelineRobustnessBufferBehavior::eRobustBufferAccess2;
            robustnessCI.images = vk::PipelineRobustnessImageBehavior::eRobustImageAccess2;
            robustnessCI.vertexInputs = vk::PipelineRobustnessBufferBehavior::eRobustBufferAccess2;
        }

        // Color Attachments
        std::array<vk::Format, GraphicsPipelineDesc::kMaxColorAttachments> colorAttachmentFormats;
        for (uint32_t i = 0; i < desc.colorAttachments.size(); ++i)
        {
            colorAttachmentFormats[i] = desc.colorAttachments[i].format;
        }
        vk::PipelineRenderingCreateInfo renderingCI{
            .pNext = usingRobustness ? &robustnessCI : nullptr,
            .colorAttachmentCount = static_cast<uint32_t>(desc.colorAttachments.size()),
            .pColorAttachmentFormats = colorAttachmentFormats.data(),
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
                .pName = desc.entryPointVertex.c_str(),
            });
        }
        if (fragmentShaderModule)
        {
            shaderStages.push_back(vk::PipelineShaderStageCreateInfo{
                .stage = vk::ShaderStageFlagBits::eFragment,
                .module = *fragmentShaderModule,
                .pName = desc.entryPointFragment.c_str(),
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
            .depthTestEnable = desc.depthTestEnable,
            .depthWriteEnable = desc.depthWriteEnable,
            .depthCompareOp = desc.depthCompareOp,
            .depthBoundsTestEnable = vk::False,
            .stencilTestEnable = vk::False,
            .front = {},
            .back = {},
            .minDepthBounds = 0.0f,
            .maxDepthBounds = 1.0f,
        };

        // Color Blending
        std::array<vk::PipelineColorBlendAttachmentState, GraphicsPipelineDesc::kMaxColorAttachments>
            colorBlendAttachments;
        for (uint32_t i = 0; i < desc.colorAttachments.size(); ++i)
        {
            colorBlendAttachments[i] = vk::PipelineColorBlendAttachmentState{
                .blendEnable = desc.colorAttachments[i].blendEnabled,
                .srcColorBlendFactor = desc.colorAttachments[i].srcRgbBlendFactor,
                .dstColorBlendFactor = desc.colorAttachments[i].dstRgbBlendFactor,
                .colorBlendOp = desc.colorAttachments[i].rgbBlendOp,
                .srcAlphaBlendFactor = desc.colorAttachments[i].srcAlphaBlendFactor,
                .dstAlphaBlendFactor = desc.colorAttachments[i].dstAlphaBlendFactor,
                .alphaBlendOp = desc.colorAttachments[i].alphaBlendOp,
                .colorWriteMask = desc.colorAttachments[i].colorWriteMask,
            };
        }
        vk::PipelineColorBlendStateCreateInfo colorBlendCI{
            .logicOpEnable = vk::False,
            .logicOp = vk::LogicOp::eCopy,
            .attachmentCount = static_cast<uint32_t>(desc.colorAttachments.size()),
            .pAttachments = colorBlendAttachments.data(),
        };

        // Dynamic States
        std::array<vk::DynamicState, 2> dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
        vk::PipelineDynamicStateCreateInfo dynamicStateCI{
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data(),
        };
        auto* layout = m_PipelineLayoutPool.get(desc.pipelineLayout);

        // Pipeline Creation
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

    Holder<PipelineLayoutHandle> Device::createPipelineLayout(PipelineLayoutDesc desc, Queue* queue,
                                                              std::string_view debugName)
    {
        std::vector<vk::DescriptorSetLayout> descriptorLayouts;
        descriptorLayouts.reserve(desc.descriptorLayouts.size());
        for (auto& dLayoutHandle: desc.descriptorLayouts)
        {
            descriptorLayouts.push_back(*m_DescriptorSetLayoutPool.get(dLayoutHandle));
        }

        // TODO think of a cleaner way to do this...
        std::vector<vk::PushConstantRange> pushConstantRanges;
        pushConstantRanges.reserve(desc.pushConstantRanges.size());
        for (auto& range: desc.pushConstantRanges)
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

    Holder<TextureHandle> Device::createTexture(
        const TextureDesc& desc, Queue* queue, std::string_view debugName)
    {
        assert(desc.mipLevels > 0);
        assert(desc.arrayLayers > 0);
        assert(desc.dimensions.width > 0);
        assert(desc.dimensions.height > 0);
        assert(desc.dimensions.depth > 0);

        assert(!(desc.generateMipmaps && desc.mipLevels > 1) && "Cannot generate partial mip chains.");

        auto mipLevels = desc.mipLevels;
        if (desc.generateMipmaps)
        {
            assert(desc.subresources.size() == 1 && "Initial data must be provided to generate mip maps");
            assert(desc.mipLevels == 1 && "Cannot manually specify mip levels when generating mip maps");
            assert(
                desc.usage & vk::ImageUsageFlagBits::eTransferDst &&
                "Mipmapped textures must have transfer dst usage.");
            assert(
                desc.usage & vk::ImageUsageFlagBits::eTransferSrc &&
                "Mipmapped textures must have transfer src usage.");

            mipLevels = static_cast<uint32_t>(std::log2(std::max(desc.dimensions.width, desc.dimensions.height))) + 1;
        }
        if (!desc.subresources.empty()) // has some sort of initial data
        {
            assert(desc.subresources.size() == desc.mipLevels * desc.arrayLayers && "Subresource count must match mipLevels * arrayLayers");
            assert(desc.usage & vk::ImageUsageFlagBits::eTransferDst && "Initial data requires transfer dst usage");
        }

        auto qfp = queue ? queue->familyIndex() : m_GraphicsQueue.familyIndex();
        vk::ImageCreateInfo imageCI{
            .imageType = vk::ImageType::e2D,
            .format = desc.format,
            .extent = desc.dimensions,
            .mipLevels = mipLevels,
            .arrayLayers = desc.arrayLayers,
            .samples = desc.samples,
            .tiling = vk::ImageTiling::eOptimal,
            .usage = desc.usage,
            .sharingMode = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &qfp,
            .initialLayout = vk::ImageLayout::eUndefined,
        };

        VulkanImage image = m_Allocator.createImage(imageCI, desc.memoryProperties);

        vk::ImageAspectFlags aspectFlags = vk::ImageAspectFlagBits::eColor;
        if (desc.format == vk::Format::eD32Sfloat || desc.format == vk::Format::eD32SfloatS8Uint ||
            desc.format == vk::Format::eD24UnormS8Uint)
        {
            aspectFlags = vk::ImageAspectFlagBits::eDepth;
        }
        image.aspectFlags_ = aspectFlags;

        vk::ImageViewCreateInfo viewCI{
            .image = image.image_,
            .viewType = vk::ImageViewType::e2D,
            .format = desc.format,
            .subresourceRange = {aspectFlags, 0, mipLevels, 0, desc.arrayLayers},
        };

        vkCheck(m_Device->createImageView(&viewCI, nullptr, &image.view_));

        if (!debugName.empty())
        {
            setDebugName(*m_Device, image.image_, debugName);
        }

        TextureHandle handle = m_TexturePool.create(std::move(image));

        if (m_UseBindless)
        {
            if (m_TexturePool.get(handle)->isStorageImage())
            {
                updateBindlessStorageImage(handle.index(), m_TexturePool.get(handle)->view_);
            }
            else if (m_TexturePool.get(handle)->isSampledImage())
            {
                updateBindlessSampledImage(handle.index(), m_TexturePool.get(handle)->view_);
            }
        }

        if (!desc.subresources.empty())
        {
            assert(aspectFlags == vk::ImageAspectFlagBits::eColor && "Non-color aspect for initial data");
            assert(desc.type == vk::ImageType::e2D && "Non-2D image for initial data");
            queue = queue ? queue : &m_GraphicsQueue;
            queue->uploadTexture2DData(handle, desc.subresources, desc.dimensions, mipLevels, desc.arrayLayers,
                                       desc.format, desc.generateMipmaps);
        }

        return {this, queue, handle};
    }

    Holder<BufferHandle> Device::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                                              vk::MemoryPropertyFlags memFlags, Queue* queue,
                                              std::string_view debugName)
    {
        assert(size > 0);

        // TODO assert that sizes are within physical device limits

        auto qfp = queue ? queue->familyIndex() : m_GraphicsQueue.familyIndex();
        vk::BufferCreateInfo bufferCI{
            .size = size,
            .usage = usage,
            .sharingMode = vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = 1,
            .pQueueFamilyIndices = &qfp,
        };

        VulkanBuffer buffer = m_Allocator.createBuffer(bufferCI, memFlags);

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

    Holder<ShaderModuleHandle> Device::createShaderModule(std::span<const uint32_t> code, Queue* queue,
                                                          std::string_view debugName)
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

    Holder<SamplerHandle> Device::createSampler(SamplerDesc desc,
                                                Queue* queue, std::string_view debugName)
    {
        vk::SamplerCreateInfo samplerCI{
            .magFilter = desc.magFilter,
            .minFilter = desc.minFilter,
            .mipmapMode = desc.mipmapMode,
            .addressModeU = desc.addressModeU,
            .addressModeV = desc.addressModeV,
            .addressModeW = desc.addressModeW,
            .anisotropyEnable = desc.anisotropyEnable,
            .maxAnisotropy = std::min(desc.maxAnisotropy, m_PhysicalDevice.getProperties().limits.maxSamplerAnisotropy),
            .compareEnable = vk::False,
            .compareOp = vk::CompareOp::eAlways,
            .minLod = desc.minLod,
            .maxLod = desc.maxLod,
        };

        vk::Sampler sampler = vkCheck(m_Device->createSampler(samplerCI));
        if (!debugName.empty())
        {
            setDebugName(*m_Device, sampler, debugName);
        }

        auto handle = m_SamplerPool.create(std::move(sampler));
        if (m_UseBindless)
        {
            updateBindlessSampler(handle.index(), *m_SamplerPool.get(handle));
        }

        return {this, queue, handle};
    }

    void Device::destroyComputePipeline(ComputePipelineHandle handle, Queue* queue)
    {
        auto pipeline = *m_ComputePipelinePool.get(handle);
        queue = queue ? queue : &m_GraphicsQueue;

        queue->deferredDestroy([=, device = *m_Device]() {
            device.destroyPipeline(pipeline.handle);
        });

        m_ComputePipelinePool.destroy(handle);
    }

    void Device::destroyGraphicsPipeline(GraphicsPipelineHandle handle, Queue* queue)
    {
        auto pipeline = *m_GraphicsPipelinePool.get(handle);
        queue = queue ? queue : &m_GraphicsQueue;

        queue->deferredDestroy([=, device = *m_Device, pipeline = pipeline]() {
            device.destroyPipeline(pipeline.handle);
        });

        m_GraphicsPipelinePool.destroy(handle);
    }

    void Device::destroyPipelineLayout(PipelineLayoutHandle handle, Queue* queue)
    {
        auto layout = *m_PipelineLayoutPool.get(handle);
        queue = queue ? queue : &m_GraphicsQueue;

        queue->deferredDestroy([=, device = *m_Device, layout = layout]() {
            device.destroyPipelineLayout(layout.layout);
        });

        m_PipelineLayoutPool.destroy(handle);
    }

    void Device::destroyTexture(TextureHandle handle, Queue* queue)
    {
        auto& texture = *m_TexturePool.get(handle);
        queue = queue ? queue : &m_GraphicsQueue;

        if (texture.isStorageImage())
        {
            updateBindlessStorageImage(handle.index(), VK_NULL_HANDLE);
        }
        else if (texture.isSampledImage())
        {
            updateBindlessSampledImage(handle.index(), VK_NULL_HANDLE);
        }

        queue->deferredDestroy(
            [=, device = *m_Device, allocator = &m_Allocator, image = texture.image_, view = texture.view_, allocation =
                texture.allocation_]() {
                device.destroyImageView(view);
                allocator->destroyImage(allocation, image);
            });

        m_TexturePool.destroy(handle);
    }

    void Device::destroyBuffer(BufferHandle handle, Queue* queue)
    {
        auto& buffer = *m_BufferPool.get(handle);
        queue = queue ? queue : &m_GraphicsQueue;

        queue->deferredDestroy([=, device = *m_Device, allocator = &m_Allocator, buffer = buffer]() {
            allocator->destroyBuffer(buffer.allocation_, buffer.buffer_);
        });

        m_BufferPool.destroy(handle);
    }

    void Device::destroyDescriptorSetLayout(DescriptorSetLayoutHandle handle, Queue* queue)
    {
        auto& layout = *m_DescriptorSetLayoutPool.get(handle);
        queue = queue ? queue : &m_GraphicsQueue;

        queue->deferredDestroy([=, device = *m_Device, layout = layout]() {
            device.destroyDescriptorSetLayout(layout);
        });

        m_DescriptorSetLayoutPool.destroy(handle);
    }

    void Device::destroyShaderModule(ShaderModuleHandle handle, Queue* queue)
    {
        auto& shader = *m_ShaderModulePool.get(handle);
        queue = queue ? queue : &m_GraphicsQueue;

        queue->deferredDestroy([=, device = *m_Device, shader = shader]() {
            device.destroyShaderModule(shader);
        });

        m_ShaderModulePool.destroy(handle);
    }

    void Device::destroySampler(SamplerHandle handle, Queue* queue)
    {
        auto& sampler = *m_SamplerPool.get(handle);
        queue = queue ? queue : &m_GraphicsQueue;

        updateBindlessSampler(handle.index(), VK_NULL_HANDLE);
        queue->deferredDestroy([=, device = *m_Device, sampler = sampler]() {
            device.destroySampler(sampler);
        });

        m_SamplerPool.destroy(handle);
    }

    UniqueCommandPool Device::createUniqueCommandPool(CommandPoolFlags flags, uint32_t queueFamilyIndex,
                                                      std::string_view debugName)
    {
        vk::CommandPoolCreateInfo commandPoolCI{
            .flags = flags == CommandPoolFlags::ResetCommandBuffer
                         ? vk::CommandPoolCreateFlagBits::eResetCommandBuffer
                         : vk::CommandPoolCreateFlagBits::eTransient,
            .queueFamilyIndex = queueFamilyIndex,
        };

        auto commandPool = vkCheck(m_Device->createCommandPoolUnique(commandPoolCI));

        if (!debugName.empty())
        {
            setDebugName(*m_Device, *commandPool, debugName.data());
        }

        return UniqueCommandPool{
            std::move(commandPool),
            queueFamilyIndex
        };
    }

    std::vector<UniqueCommandPool> Device::createUniqueCommandPools(CommandPoolFlags flags, uint32_t queueFamilyIndex,
                                                                    uint32_t count, std::string_view debugName)
    {
        vk::CommandPoolCreateInfo commandPoolCI{
            .flags = flags == CommandPoolFlags::ResetCommandBuffer
                         ? vk::CommandPoolCreateFlagBits::eResetCommandBuffer
                         : vk::CommandPoolCreateFlagBits::eTransient,
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
            result.push_back(UniqueCommandPool{
                std::move(commandPool),
                queueFamilyIndex
            });
        }

        return result;
    }

    CommandBuffer Device::allocateCommandBuffer(UniqueCommandPool& commandPool, CommandBufferLevel level,
                                                std::string_view debugName)
    {
        vk::CommandBufferAllocateInfo cmdAllocCI{
            .commandPool = *commandPool.m_CommandPool,
            .level = level == CommandBufferLevel::Primary
                         ? vk::CommandBufferLevel::ePrimary
                         : vk::CommandBufferLevel::eSecondary,
            .commandBufferCount = 1,
        };
        auto cmdBuffer = vkCheck(m_Device->allocateCommandBuffers(cmdAllocCI)).front();

        if (!debugName.empty())
        {
            setDebugName(*m_Device, cmdBuffer, debugName.data());
        }

        return {this, cmdBuffer};
    }

    std::vector<CommandBuffer> Device::allocateCommandBuffers(UniqueCommandPool& commandPool, CommandBufferLevel level,
                                                              uint32_t count, std::string_view debugName)
    {
        vk::CommandBufferAllocateInfo cmdAllocCI{
            .commandPool = *commandPool.m_CommandPool,
            .level = level == CommandBufferLevel::Primary
                         ? vk::CommandBufferLevel::ePrimary
                         : vk::CommandBufferLevel::eSecondary,
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

    void Device::initBindless()
    {
        assert(m_Device);

        // Create Descriptor Set Layout
        std::array binding{
            vk::DescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = vk::DescriptorType::eStorageImage,
                .descriptorCount = 10'000,
                .stageFlags = vk::ShaderStageFlagBits::eAll,
            },
            vk::DescriptorSetLayoutBinding{
                .binding = 1,
                .descriptorType = vk::DescriptorType::eSampledImage,
                .descriptorCount = 10'000,
                .stageFlags = vk::ShaderStageFlagBits::eAll,
            },
            vk::DescriptorSetLayoutBinding{
                .binding = 2,
                .descriptorType = vk::DescriptorType::eSampler,
                .descriptorCount = 10'000,
                .stageFlags = vk::ShaderStageFlagBits::eAll,
            }
        };

        std::array<vk::DescriptorBindingFlags, binding.size()> flags;
        flags.fill(vk::DescriptorBindingFlagBits::ePartiallyBound
                   | vk::DescriptorBindingFlagBits::eUpdateAfterBind);

        vk::DescriptorSetLayoutBindingFlagsCreateInfo flagsCI{
            .bindingCount = static_cast<uint32_t>(flags.size()),
            .pBindingFlags = flags.data()
        };

        vk::DescriptorSetLayoutCreateInfo layoutCI{
            .pNext = &flagsCI,
            .flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
            .bindingCount = static_cast<uint32_t>(binding.size()),
            .pBindings = binding.data(),
        };
        auto layout = vkCheck(m_Device->createDescriptorSetLayout(layoutCI));
        setDebugName(*m_Device, layout, "Bindless Descriptor Set Layout");
        auto handle = m_DescriptorSetLayoutPool.create(std::move(layout));
        m_BindlessLayoutHandle = {this, &m_GraphicsQueue, handle};

        // Create Descriptor Pool
        std::array poolSizes = {
            vk::DescriptorPoolSize{
                .type = vk::DescriptorType::eStorageImage,
                .descriptorCount = 10'000,
            },
            vk::DescriptorPoolSize{
                .type = vk::DescriptorType::eSampledImage,
                .descriptorCount = 10'000,
            },
            vk::DescriptorPoolSize{
                .type = vk::DescriptorType::eSampler,
                .descriptorCount = 10'000,
            }
        };
        vk::DescriptorPoolCreateInfo poolCI{
            .flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind |
                     vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = 1,
            .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
            .pPoolSizes = poolSizes.data(),
        };
        m_BindlessPool = vkCheck(m_Device->createDescriptorPoolUnique(poolCI));
        setDebugName(*m_Device, *m_BindlessPool, "Bindless Descriptor Pool");

        // Create Descriptor Set
        vk::DescriptorSetAllocateInfo allocCI{
            .descriptorPool = *m_BindlessPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &layout,
        };
        m_GlobalDescriptorSet = std::move(vkCheck(m_Device->allocateDescriptorSetsUnique(allocCI)).front());
        setDebugName(*m_Device, *m_GlobalDescriptorSet, "Bindless Descriptor Set");

        // Create Pipeline Layouts
        PushConstantsInfo computePCI{
            .offset = 0,
            .size = sizeof(ComputePushConstants),
            .stages = vk::ShaderStageFlagBits::eCompute,
        };
        m_BindlessComputePipelineLayout = createPipelineLayout({
                                                                   .descriptorLayouts = {{m_BindlessLayoutHandle}},
                                                                   .pushConstantRanges = {{computePCI}},
                                                               }, &m_GraphicsQueue, "Bindless Compute Pipeline Layout");
        PushConstantsInfo graphicsPCI{
            .offset = 0,
            .size = sizeof(GraphicsPushConstants),
            .stages = vk::ShaderStageFlagBits::eAllGraphics,
        };
        m_BindlessGraphicsPipelineLayout = createPipelineLayout({
                                                                    .descriptorLayouts = {{m_BindlessLayoutHandle}},
                                                                    .pushConstantRanges = {{graphicsPCI}},
                                                                }, &m_GraphicsQueue,
                                                                "Bindless Graphics Pipeline Layout");
    }

    void Device::updateBindlessStorageImage(uint32_t index, vk::ImageView view)
    {
        assert(m_GlobalDescriptorSet);

        if ((view == VK_NULL_HANDLE || view == nullptr) && !m_DeviceInfo.hasNullDescriptor)
        {
            // if hasNullDescriptor isn't supported, we could fallback to a default texture.
            return;
        }
        if (!m_GlobalDescriptorSet) [[unlikely]]
        {
            return;
        }

        vk::DescriptorImageInfo imageInfo{
            .imageView = view,
            .imageLayout = vk::ImageLayout::eGeneral,
        };

        vk::WriteDescriptorSet write{
            .dstSet = *m_GlobalDescriptorSet,
            .dstBinding = 0,
            .dstArrayElement = index,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eStorageImage,
            .pImageInfo = &imageInfo,
        };

        m_Device->updateDescriptorSets(1, &write, 0, nullptr);
    }

    void Device::updateBindlessSampledImage(uint32_t index, vk::ImageView view)
    {
        assert(m_GlobalDescriptorSet);

        if ((view == VK_NULL_HANDLE || view == nullptr) && !m_DeviceInfo.hasNullDescriptor)
        {
            // if hasNullDescriptor isn't supported, we could fallback to a default texture.
            return;
        }
        if (!m_GlobalDescriptorSet) [[unlikely]]
        {
            return;
        }

        vk::DescriptorImageInfo imageInfo{
            .imageView = view,
            .imageLayout = vk::ImageLayout::eGeneral,
        };

        vk::WriteDescriptorSet write{
            .dstSet = *m_GlobalDescriptorSet,
            .dstBinding = 1,
            .dstArrayElement = index,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eSampledImage,
            .pImageInfo = &imageInfo,
        };

        m_Device->updateDescriptorSets(1, &write, 0, nullptr);
    }

    void Device::updateBindlessSampler(uint32_t index, vk::Sampler sampler)
    {
        assert(m_GlobalDescriptorSet);

        if ((sampler == VK_NULL_HANDLE || sampler == nullptr))
        {
            // You can't write null descriptors to samplers even with Robustness2 nullDescriptor. Use a default sampler here.
            const auto* defaultSampler = m_SamplerPool.get(m_DefaultNullSampler);
            assert(defaultSampler);
            sampler = *defaultSampler;
        }
        if (!m_GlobalDescriptorSet) [[unlikely]]
        {
            return;
        }

        vk::DescriptorImageInfo imageInfo{
            .sampler = sampler,
        };

        vk::WriteDescriptorSet write{
            .dstSet = *m_GlobalDescriptorSet,
            .dstBinding = 2,
            .dstArrayElement = index,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eSampler,
            .pImageInfo = &imageInfo,
        };

        m_Device->updateDescriptorSets(1, &write, 0, nullptr);
    }
}
