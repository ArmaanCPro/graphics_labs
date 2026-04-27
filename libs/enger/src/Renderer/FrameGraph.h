#pragma once

#include "enger_export.h"
#include "Renderpass.h"

#include "vulkan/GpuResourceTypes.h"

namespace enger::fg
{
    struct ComputedBarriers
    {
        std::vector<CommandBuffer::TransferTextureDesc> imageBarriers;
        // add buffer barriers later as well
    };

    class ENGER_EXPORT FrameGraph
    {
    public:
        void addPass(RenderPassDesc desc)
        {
            m_Passes.push_back(std::move(desc));
        }

        void compile(Device& device);
        void execute(CommandBuffer& cmd, Device& device);

        // Swapchain image gets state reset to eUndefined
        void reset(Device& device, TextureHandle swapchainImage);

    private:
        std::vector<RenderPassDesc> m_Passes;
        std::vector<ComputedBarriers> m_Barriers;

        vk::RenderingInfo buildRenderingInfo(
            const RenderPassDesc& pass,
            Device& device,
            std::vector<vk::RenderingAttachmentInfo>& outColorInfos,
            std::optional<vk::RenderingAttachmentInfo>& outDepthInfo
        ) const;

        void submitBarriers(
            CommandBuffer& cmd,
            const ComputedBarriers& barriers,
            Device& device
        ) const;
    };
}
