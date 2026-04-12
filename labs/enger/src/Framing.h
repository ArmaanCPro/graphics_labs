#pragma once

#include <cstdint>
#include "vulkan/Commands.h"

namespace enger::framing
{
    static constexpr auto FRAMES_IN_FLIGHT = 2;

    struct FrameContext
    {
        CommandBuffer& cmd;
        uint32_t swapchainImageIndex;
        TextureHandle swapchainImageHandle;
        vk::Extent2D swapchainExtent;
        uint32_t frameIndex;
    };

    class IFrameLayer
    {
    public:
        virtual ~IFrameLayer() = default;

        virtual void draw(FrameContext& ctx) = 0;

        virtual void onResize([[maybe_unused]] uint32_t width, [[maybe_unused]] uint32_t height) {};
    };
}
