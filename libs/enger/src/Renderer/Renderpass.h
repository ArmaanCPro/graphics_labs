#pragma once

#include "Resources/Resources.h"

#include <vector>

#include "vulkan/Commands.h"
#include "vulkan/Device.h"

#include "enger_export.h"

namespace enger::fg
{
    enum class PassType
    {
        Graphics,
        Compute
    };

    struct ENGER_EXPORT AttachmentDesc
    {
        TextureHandle texture;
        vk::AttachmentLoadOp loadOp = vk::AttachmentLoadOp::eClear;
        vk::AttachmentStoreOp storeOp = vk::AttachmentStoreOp::eStore;
        vk::ClearValue clearValue{};
        std::optional<TextureHandle> resolveImage = std::nullopt;
        vk::ResolveModeFlagBits resolveMode = vk::ResolveModeFlagBits::eAverage;
    };

    struct ENGER_EXPORT RenderPassDesc
    {
        PassType type = PassType::Graphics;

        // Resource declarations -> used for barriers
        std::vector<TextureHandle> colorWrites;
        std::optional<TextureHandle> depthWrite;
        std::vector<TextureHandle> shaderReads;

        // Attachment config (only for graphics passes)
        std::vector<AttachmentDesc> colorAttachments;
        std::optional<AttachmentDesc> depthAttachment;

        std::function<void(CommandBuffer&)> execute;

        // Debug
        const char* name = nullptr;
    };

    struct ENGER_EXPORT PostProcessPassDesc
    {
        const char* name = nullptr;
        TextureHandle input;
        TextureHandle output; // could be swapchain image
        GraphicsPipelineHandle graphicsPipeline;
        PipelineLayoutHandle pipelineLayout;
        uint32_t pushConstantsSize; // Could make this PushConstantsInfo, but it is always the Fragment stage for post-process, so all we need is the size (no offset)
        std::array<std::byte, 128> pushConstantsData; // fixed-size blob
    };

    inline RenderPassDesc MakePostProcessPass(const PostProcessPassDesc& desc, Device& device)
    {
        return RenderPassDesc{
            .colorWrites = {desc.output},
            .shaderReads = {desc.input},
            .colorAttachments = {
                AttachmentDesc{
                    .texture = desc.output,
                    .loadOp = vk::AttachmentLoadOp::eDontCare,
                    .storeOp = vk::AttachmentStoreOp::eStore,
            }},
            .execute = [desc, &device](CommandBuffer& cmd) {
                cmd.bindGraphicsPipeline(desc.graphicsPipeline);
                cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, desc.pipelineLayout, 0, {{device.bindlessDescriptorSet()}});

                const auto* output = device.getImage(desc.output);
                EASSERT(output);

                vk::Viewport viewport{
                    0.0f, 0.0f, static_cast<float>(output->extent_.width), static_cast<float>(output->extent_.height), 0.0f, 1.0f
                };
                cmd.setViewport(std::move(viewport));
                cmd.setScissor(vk::Rect2D{0, 0, output->extent_.width, output->extent_.height});
                cmd.pushConstants(desc.pipelineLayout, vk::ShaderStageFlagBits::eFragment, 0, desc.pushConstantsSize, desc.pushConstantsData.data());
                cmd.draw(3, 1, 0, 0); // draws a FullScreen Triangle
            },
            .name = desc.name,
        };
    }
}
