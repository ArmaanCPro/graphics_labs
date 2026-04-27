#include "FrameGraph.h"

#include <unordered_map>

#include "vulkan/Device.h"

namespace enger::fg
{
    void FrameGraph::compile(Device& device)
    {
        m_Barriers.clear();
        m_Barriers.reserve(m_Passes.size());

        for (auto& pass : m_Passes)
        {
            ComputedBarriers barriers;

            // Helper to emit transitions if needed
            auto process = [&](TextureHandle handle, ImageState required) {
                auto* img = device.getImage(handle);
                EASSERT(img);

                const auto& current = img->state_;

                if (current.layout != required.layout)
                {
                    barriers.imageBarriers.push_back({
                        .handle = handle,
                        .srcLayout = current.layout,
                        .dstLayout = required.layout,
                        .srcAccess = current.access,
                        .dstAccess = required.access,
                        .srcStage = current.stage,
                        .dstStage = required.stage,
                    });
                }
                img->state_ = required;
            };

            for (auto& img : pass.shaderReads)
            {
                process(img, shaderReadState());
            }

            for (auto& img : pass.colorWrites)
            {
                process(img, colorWriteState());
            }

            if (pass.depthWrite)
            {
                process(pass.depthWrite.value(), depthWriteState());
            }

            m_Barriers.push_back(std::move(barriers));
        }
    }

    void FrameGraph::execute(CommandBuffer& cmd, Device& device)
    {
        EASSERT(m_Barriers.size() == m_Passes.size());

        for (size_t i = 0; i < m_Passes.size(); ++i)
        {
            const auto& pass = m_Passes[i];
            const auto& barriers = m_Barriers[i];

            // Barriers
            submitBarriers(cmd, barriers, device);

            // Recording
            if (pass.type == PassType::Graphics)
            {
                std::vector<vk::RenderingAttachmentInfo> colorInfos;
                std::optional<vk::RenderingAttachmentInfo> depthInfo;
                auto renderingInfo = buildRenderingInfo(pass, device, colorInfos, depthInfo);

                cmd.beginRendering(renderingInfo);
                pass.execute(cmd);
                cmd.endRendering();
            }
            else if (pass.type == PassType::Compute)
            {
                // Compute -> no Rendering
                pass.execute(cmd);
            }
            else
            {
                EASSERT(false, "Invalid pass type");
            }
        }
    }

    void FrameGraph::reset(Device& device, TextureHandle swapchainImage)
    {
        m_Passes.clear();
        m_Barriers.clear();

        // Present invalidates swapchain image layout
        device.getImage(swapchainImage)->state_ = ImageState
        {
            vk::ImageLayout::eUndefined, vk::AccessFlagBits2::eNone, vk::PipelineStageFlagBits2::eColorAttachmentOutput
        };
    }

    vk::RenderingInfo FrameGraph::buildRenderingInfo(const RenderPassDesc& pass, Device& device,
        std::vector<vk::RenderingAttachmentInfo>& outColorInfos,
        std::optional<vk::RenderingAttachmentInfo>& outDepthInfo) const
    {
        outColorInfos.reserve(pass.colorAttachments.size());

        for (const auto& att : pass.colorAttachments)
        {
            const auto* img = device.getImage(att.texture);
            EASSERT(img);

            vk::RenderingAttachmentInfo info{
                .imageView = img->view_,
                .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .loadOp = att.loadOp,
                .storeOp = att.storeOp,
                .clearValue = att.clearValue,
            };

            if (att.resolveImage.has_value())
            {
                EASSERT(std::ranges::contains(pass.colorWrites, att.resolveImage.value()), "Make sure the resolve attachment appears in the colorWrites too!");
                const auto* resolveImg = device.getImage(att.resolveImage.value());
                EASSERT(resolveImg);

                info.resolveMode = att.resolveMode;
                info.resolveImageView = resolveImg->view_;
                info.resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal;
            }

            outColorInfos.push_back(info);
        }

        vk::Extent2D extent{};
        if (!pass.colorAttachments.empty())
        {
            const auto* img = device.getImage(pass.colorAttachments.front().texture);
            EASSERT(img);
            extent = {img->extent_.width, img->extent_.height};
        }

        if (pass.depthAttachment.has_value())
        {
            const auto& att = *pass.depthAttachment;
            const auto* img = device.getImage(att.texture);
            EASSERT(img);

            outDepthInfo = vk::RenderingAttachmentInfo{
                .imageView = img->view_,
                .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
                .loadOp = att.loadOp,
                .storeOp = att.storeOp,
                .clearValue = att.clearValue,
            };

            if (extent.width == 0 || extent.height == 0)
                extent = {img->extent_.width, img->extent_.height};
        }

        return vk::RenderingInfo{
            .renderArea = vk::Rect2D{
                .offset = {0, 0},
                .extent = extent,
            },
            .layerCount = 1,
            .colorAttachmentCount = static_cast<uint32_t>(outColorInfos.size()),
            .pColorAttachments = outColorInfos.data(),
            .pDepthAttachment = outDepthInfo.has_value() ? &outDepthInfo.value() : nullptr,
        };
    }

    void FrameGraph::submitBarriers(CommandBuffer& cmd, const ComputedBarriers& barriers, Device& device) const
    {
        if (barriers.imageBarriers.empty())
            return;

        std::vector<vk::ImageMemoryBarrier2> vkBarriers;
        vkBarriers.reserve(barriers.imageBarriers.size());

        for (const auto& barrier : barriers.imageBarriers)
        {
            auto* img = device.getImage(barrier.handle);
            EASSERT(img);

            vkBarriers.push_back(vk::ImageMemoryBarrier2{
                .srcStageMask = barrier.srcStage.value(),
                .srcAccessMask = barrier.srcAccess.value(),
                .dstStageMask = barrier.dstStage.value(),
                .dstAccessMask = barrier.dstAccess.value(),
                .oldLayout = barrier.srcLayout,
                .newLayout = barrier.dstLayout,
                .image = img->image_,
                .subresourceRange = {
                    .aspectMask = img->aspectFlags_,
                    .baseMipLevel = 0,
                    .levelCount = vk::RemainingMipLevels,
                    .baseArrayLayer = 0,
                    .layerCount = vk::RemainingArrayLayers,
                }
            });
        }

        vk::DependencyInfo dependencyInfo{
            .imageMemoryBarrierCount = static_cast<uint32_t>(vkBarriers.size()),
            .pImageMemoryBarriers = vkBarriers.data(),
        };

        cmd.transitionImages(dependencyInfo);
    }
}
