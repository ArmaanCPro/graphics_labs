#include "Imgui.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>


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

    ImguiLayer::ImguiLayer(Instance& instance, Device &device, GlfwWindow& window, SwapChain& swapchain)
        :
        m_Device(device),
        m_Swapchain(swapchain),
        m_Window(window)
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


        ImGui_ImplGlfw_InitForVulkan(window.nativeHandle(), true);

        auto swapchainFormat = static_cast<VkFormat>(swapchain.swapChainUnormFormat());

        ImGui_ImplVulkan_InitInfo initInfo{
            .Instance = instance.instance(),
            .PhysicalDevice = device.physicalDevice(),
            .Device = device.device(),
            .Queue = device.graphicsQueue().queue(),
            .DescriptorPool = m_DescriptorAllocator.pool_,
            .MinImageCount = swapchain.numSwapChainImages(),
            .ImageCount = swapchain.numSwapChainImages(),

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

        // Styling
        ImGui::StyleColorsDark();

        float dpiScale = m_Window.getDpiScale();
        io.Fonts->Clear();

        ImFontConfig fontcfg;
        fontcfg.OversampleH = 2;
        fontcfg.OversampleV = 2;
        fontcfg.PixelSnapH = true;
        io.Fonts->AddFontFromFileTTF("assets/fonts/Inter/Inter_18pt-Medium.ttf", 16.0f * dpiScale, &fontcfg, io.Fonts->GetGlyphRangesDefault());
        io.Fonts->Build();

        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(dpiScale);
    }

    ImguiLayer::~ImguiLayer()
    {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        m_DescriptorAllocator.destroyPool(m_Device.device());
    }

    void ImguiLayer::beginFrame()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        ImGui::NewFrame();
    }

    void ImguiLayer::endFrame(framing::FrameContext& frameContext)
    {
        ImGui::Render();

        vk::RenderingAttachmentInfo colorAttachmentInfo{
            .imageView = m_Swapchain.swapChainUnormImageView(frameContext.swapchainImageIndex),
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp = vk::AttachmentLoadOp::eLoad,
            .storeOp = vk::AttachmentStoreOp::eStore,
        };

        auto extent = frameContext.swapchainExtent;
        vk::RenderingInfo renderingInfo{
            .renderArea = vk::Rect2D{0, 0, extent.width, extent.height},
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentInfo,
        };

        frameContext.cmd.beginRendering(renderingInfo);

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), frameContext.cmd.get());

        frameContext.cmd.endRendering();
    }

    void ImguiLayer::postRenderFinished()
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    void ImguiLayer::onResize([[maybe_unused]] uint32_t width, [[maybe_unused]] uint32_t height)
    {
        ImGui_ImplVulkan_SetMinImageCount(m_Swapchain.numSwapChainImages());
    }
}
