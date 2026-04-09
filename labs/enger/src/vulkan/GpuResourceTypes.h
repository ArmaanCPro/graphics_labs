#pragma once

#include "vk.h"

namespace enger
{
    struct VulkanImage final
    {
        vk::Image image_;
        vk::ImageView view_;
        VmaAllocation allocation_;
        vk::Extent3D extent_;
        vk::ImageUsageFlags usage_;
        vk::Format format_;
    };
}