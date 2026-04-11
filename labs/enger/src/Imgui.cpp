#include "Imgui.h"

#include "vulkan/Device.h"
#include "vulkan/Instance.h"
#include "vulkan/SwapChain.h"

#include "vulkan/vk.h"

namespace enger
{
    PFN_vkVoidFunction imguiFn(const char* function_name, void* user_data)
    {
        return VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr(*(reinterpret_cast<VkInstance*>(user_data)), function_name);
    }

    ImguiLayer::ImguiLayer(Instance& instance, Device &device, GLFWwindow *window, SwapChain& swapchain)
        :
        m_Device(device),
        m_Swapchain(swapchain)
    {
        VkInstance inst = instance.instance();
        ImGui_ImplVulkan_LoadFunctions(vk::ApiVersion14, imguiFn, &inst);

        std::array<DescriptorAllocator::PoolSizeRatio, 11> poolSizes = {
            DescriptorAllocator::PoolSizeRatio{ vk::DescriptorType::eSampler, 1000.0f },
            DescriptorAllocator::PoolSizeRatio{ vk::DescriptorType::eCombinedImageSampler, 1000.0f },
            DescriptorAllocator::PoolSizeRatio{ vk::DescriptorType::eSampledImage, 1000.0f },
            DescriptorAllocator::PoolSizeRatio{ vk::DescriptorType::eStorageImage, 1000.0f },
            DescriptorAllocator::PoolSizeRatio{ vk::DescriptorType::eUniformTexelBuffer, 1000.0f },
            DescriptorAllocator::PoolSizeRatio{ vk::DescriptorType::eStorageTexelBuffer, 1000.0f },
            DescriptorAllocator::PoolSizeRatio{ vk::DescriptorType::eUniformBuffer, 1000.0f },
            DescriptorAllocator::PoolSizeRatio{ vk::DescriptorType::eStorageBuffer, 1000.0f },
            DescriptorAllocator::PoolSizeRatio{ vk::DescriptorType::eUniformBufferDynamic, 1000.0f },
            DescriptorAllocator::PoolSizeRatio{ vk::DescriptorType::eStorageBufferDynamic, 1000.0f },
            DescriptorAllocator::PoolSizeRatio{ vk::DescriptorType::eInputAttachment, 1000.0f },
        };

        m_DescriptorAllocator.initPool(device.device(), 1000, poolSizes, vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForVulkan(window, true);

        auto swapchainFormat = static_cast<VkFormat>(swapchain.swapChainFormat());

        ImGui_ImplVulkan_InitInfo initInfo{
            .Instance = instance.instance(),
            .PhysicalDevice = device.physicalDevice(),
            .Device = device.device(),
            .Queue = device.graphicsQueue().queue(),
            .DescriptorPool = m_DescriptorAllocator.pool_,
            .MinImageCount = 3, // TODO parameterize...? (many of these)
            .ImageCount = 3,

            .PipelineInfoMain = ImGui_ImplVulkan_PipelineInfo{
                .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
                .PipelineRenderingCreateInfo = VkPipelineRenderingCreateInfoKHR{
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
                    .colorAttachmentCount = 1,
                    .pColorAttachmentFormats = &swapchainFormat
                },
            },

            .UseDynamicRendering = true,
        };

        ImGui_ImplVulkan_Init(&initInfo);
    }

    ImguiLayer::~ImguiLayer()
    {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_DescriptorAllocator.destroyPool(m_Device.device());
    }

    void ImguiLayer::draw(vk::CommandBuffer cmd, vk::ImageView targetImageView)
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        ImGui::NewFrame();

        bool enable = true;
        ImGui::ShowDemoWindow(&enable);

        ImGui::Render();

        vk::RenderingAttachmentInfo colorAttachmentInfo{
            .imageView = targetImageView,
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eLoad,
            .storeOp = vk::AttachmentStoreOp::eStore,
        };

        auto extent = m_Swapchain.swapChainExtent();
        vk::RenderingInfo renderingInfo{
            .renderArea = vk::Rect2D{0, 0, extent.width, extent.height},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentInfo,
        };

        cmd.beginRendering(renderingInfo);

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

        cmd.endRendering();

        ImGui::EndFrame();

        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }
}
